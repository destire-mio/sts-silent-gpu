"""Scalar CPU reference for the tensor collector, never imported by training."""
from types import SimpleNamespace
from pathlib import Path
import time
import numpy as np
import spire
import torch
from train.model import distribution, network_inputs, views
from train.rollout import synchronize
from train.sampling import root_key


def word(seed, key, attempt, decision):
    mask = (1 << 32) - 1
    c = [decision & mask, decision >> 32, attempt & mask, attempt >> 32]
    k = [int(key[:8], 16) ^ (seed & mask), int(key[8:16], 16) ^ ((seed >> 32) & mask)]
    for _ in range(10):
        p, q = c[0] * 0xD2511F53, c[2] * 0xCD9E8D57
        c = [(q >> 32) ^ c[1] ^ k[0], q & mask, (p >> 32) ^ c[3] ^ k[1], p & mask]
        k = [(k[0] + 0x9E3779B9) & mask, (k[1] + 0xBB67AE85) & mask]
    return c[0]


def reference_sample(policy, seed, keys, attempts, decisions):
    cdf = policy.probs.cpu().numpy().cumsum(axis=1, dtype=np.float64)
    cdf /= cdf[:, -1:]
    u = np.array([(word(seed, k, a, d) + .5) / 2**32 for k, a, d in zip(keys, attempts, decisions)])
    return (u[:, None] >= cdf).sum(1).astype(np.int32)


def episode_targets(rewards, values, bootstrap, gae_lambda):
    returns, advantages = np.zeros(len(rewards), np.float32), np.zeros(len(rewards), np.float32)
    target, advantage, next_value = bootstrap, 0., bootstrap
    for i in range(len(rewards) - 1, -1, -1):
        target = float(rewards[i]) + target
        advantage = float(rewards[i]) + next_value - float(values[i]) + gae_lambda * advantage
        returns[i], advantages[i] = target, advantage
        next_value = float(values[i])
    return returns, advantages


class Rollout:
    @staticmethod
    def allocate(n, storage_mib):
        arrays = {name: np.zeros(n, dtype=dtype) for name, dtype in dict(actions=np.int32,
            log_probs=np.float32, values=np.float32, rewards=np.float32, returns=np.float32,
            advantages=np.float32, screens=np.uint8, acts=np.uint8, entropy=np.float32,
            complete=bool, valid=bool, policy_versions=np.int64).items()}
        return SimpleNamespace(**arrays, count=0, data=np.empty(storage_mib * 1024**2, np.uint8),
                               offsets=np.zeros(n + 1, np.uint64), episodes=[], timing={})


class ReferenceCollector:
    def __init__(self, size, decision_cap, storage_mib, device, gae_lambda):
        self.batch = spire.TrainingBatch(size, 1, decision_cap, [])
        self.arrays = views(self.batch)
        self.cap, self.storage_mib = decision_cap, storage_mib
        self.device, self.gae_lambda = device, gae_lambda
        self.size = size

    @torch.no_grad()
    def collect(self, model, roots, sampling_seed, attempts, decision_budget=None, *, policy_version):
        model.eval()
        start = time.perf_counter()
        budget = len(roots) * self.cap if decision_budget is None else decision_budget
        assert budget > 0
        rollout = Rollout.allocate(min(len(roots) * self.cap, budget), self.storage_mib)
        attempts = list(attempts)
        episodes = [dict(seed=root["seed"], root_key=root_key(root), prefix_decisions=len(root["candidates"]),
                         attempt=attempt, policy_version=policy_version, candidates=[], records=[], outcome=None, fault=0)
                    for root, attempt in zip(roots, attempts)]
        keys = [root_key(root) for root in roots]
        slots = list(range(min(self.size, len(roots))))
        current = dict(zip(slots, slots))
        next_episode = len(slots)
        engine_seconds = inference_seconds = storage_seconds = 0.0
        worker_before = self.batch.timing_ns.sum(axis=0).copy()

        def reset(indices, episode_ids):
            before = time.perf_counter()
            self.batch.reset_to_many(indices, [roots[i]["seed"] for i in episode_ids],
                                     [roots[i]["candidates"] for i in episode_ids])
            return time.perf_counter() - before

        engine_seconds += reset(slots, slots)
        while current:
            if rollout.count == budget:
                for episode_id in current.values():
                    episodes[episode_id].update(outcome="budget_cutoff")
                break
            active = np.array(list(current), dtype=np.int32)
            failed = [int(slot) for slot in active if self.batch.faults[slot]]
            for slot in failed:
                episode = episodes[current.pop(slot)]
                episode.update(outcome="fault", fault=int(self.batch.faults[slot]))
            active = np.array(list(current)[:budget - rollout.count], dtype=np.int32)
            if len(active):
                begin, end = rollout.count, rollout.count + len(active)
                rollout.policy_versions[begin:end] = policy_version
                before = time.perf_counter()
                code = self.batch.store(active, rollout.data, rollout.offsets[begin:end + 1])
                assert code == 0, f"Compact rollout storage exhausted: {code:#x}; increase storage_mib"
                storage_seconds += time.perf_counter() - before
                synchronize(self.device)
                before = time.perf_counter()
                inputs = network_inputs(self.arrays, self.device, active)
                logits, value = model(inputs)
                policy, entropy = distribution(logits, inputs["candidate_mask"])
                selected = reference_sample(policy, sampling_seed, [keys[current[int(slot)]] for slot in active],
                                            [attempts[current[int(slot)]] for slot in active],
                                            [len(episodes[current[int(slot)]]["records"]) for slot in active])
                log_prob = policy.log_prob(torch.tensor(selected, device=self.device)).cpu().numpy()
                rollout.values[begin:end] = value.cpu().numpy()
                rollout.entropy[begin:end] = entropy.cpu().numpy()
                synchronize(self.device)
                inference_seconds += time.perf_counter() - before
                rollout.actions[begin:end] = selected
                rollout.log_probs[begin:end] = log_prob
                rollout.screens[begin:end] = self.arrays["ids"][active, 0, 0] - 1
                rollout.acts[begin:end] = self.arrays["values"][active, 0, 0]
                for index, slot, choice in zip(range(begin, end), active, selected):
                    episode = episodes[current[int(slot)]]
                    episode["records"].append(index)
                    episode["candidates"].append(int(choice))
                before = time.perf_counter()
                self.batch.step_slots(active.tolist(), selected.tolist())
                engine_seconds += time.perf_counter() - before
                rollout.rewards[begin:end] = self.batch.rewards[active]
                rollout.count = end
                for slot in active:
                    slot = int(slot)
                    episode = episodes[current[slot]]
                    if self.batch.faults[slot]:
                        episode.update(outcome="fault", fault=int(self.batch.faults[slot]))
                    elif self.batch.done[slot] or self.batch.truncated[slot]:
                        bootstrap = 0.0
                        if self.batch.truncated[slot]:
                            synchronize(self.device)
                            before = time.perf_counter()
                            _, tail_value = model(network_inputs(self.arrays, self.device, [slot]))
                            bootstrap = float(tail_value.item())
                            synchronize(self.device)
                            inference_seconds += time.perf_counter() - before
                            outcome = "truncated"
                        else:
                            outcome = "victory" if self.batch.rewards[slot] == 1 else "death"
                        episode.update(outcome=outcome, bootstrap=bootstrap)
                        indices = np.array(episode["records"], dtype=np.int32)
                        targets, advantages = episode_targets(rollout.rewards[indices], rollout.values[indices],
                                                              bootstrap, self.gae_lambda)
                        rollout.returns[indices], rollout.advantages[indices] = targets, advantages
                        rollout.complete[indices] = outcome != "truncated"
                        rollout.valid[indices] = True
                    if episode["outcome"] is not None:
                        del current[slot]
            available = [slot for slot in range(self.size) if slot not in current]
            assigned = list(range(next_episode, min(len(roots), next_episode + len(available))))
            if assigned and rollout.count < budget:
                refill = available[:len(assigned)]
                engine_seconds += reset(refill, assigned)
                current.update(zip(refill, assigned))
                next_episode += len(assigned)
        rollout.episodes = [episode for episode in episodes if episode["outcome"] is not None]
        rollout.timing = dict(collection_seconds=time.perf_counter() - start,
                              engine_encode_seconds=engine_seconds, inference_seconds=inference_seconds,
                              storage_seconds=storage_seconds,
                              worker_seconds=((self.batch.timing_ns.sum(axis=0) - worker_before) / 1e9).tolist())
        return rollout


def winning_root(distance):
    words = iter(map(int, (Path(__file__).parent / "deep_benchmark.actions").read_text().split()))
    names = ("play", "end_turn", "choose", "potion_use", "potion_discard", "proceed", "skip")
    for seed in words:
        count = next(words)
        raw = [(next(words), next(words), next(words)) for _ in range(count)]
        if seed != 202610180018:
            continue
        game, prefix = spire.reset(seed), []
        for kind, index, target in raw[:-distance]:
            action = dict(type=names[kind])
            if kind == 0:
                action.update(card=index, target=target)
            elif kind == 2:
                action.update(option=index)
            elif kind in (3, 4):
                action.update(slot=index)
                if kind == 3:
                    action.update(target=target)
            prefix.append(game.legal_actions().index(action))
            game.step(action)
        return dict(seed=seed, candidates=prefix)
    raise AssertionError("Missing accepted winning route")
