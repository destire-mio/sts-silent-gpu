from dataclasses import dataclass
import hashlib
import json
import time

import numpy as np
import spire
import torch

from .model import distribution, network_inputs, views


def synchronize(device):
    if device.type == "cuda":
        torch.cuda.synchronize(device)
    elif device.type == "mps":
        torch.mps.synchronize()


def root_key(root):
    payload = json.dumps([root["seed"], root["candidates"]], separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def policy_rng(stream, seed, attempt=0):
    unsigned = seed & ((1 << 64) - 1)
    return np.random.default_rng(np.random.SeedSequence([stream, unsigned & 0xffffffff, unsigned >> 32, attempt]))


def sample(policy, generators):
    probabilities = policy.probs.cpu().numpy()
    uniforms = np.array([rng.random() for rng in generators])
    cumulative = probabilities.cumsum(axis=1, dtype=np.float64)
    cumulative /= cumulative[:, -1:]
    return (uniforms[:, None] >= cumulative).sum(axis=1).astype(np.int32)


def episode_targets(rewards, values, bootstrap, gae_lambda):
    returns = np.empty(len(rewards), dtype=np.float32)
    advantages = np.empty_like(returns)
    target, advantage, next_value = bootstrap, 0.0, bootstrap
    for i in range(len(rewards) - 1, -1, -1):
        target = float(rewards[i]) + target
        advantage = float(rewards[i]) + next_value - float(values[i]) + gae_lambda * advantage
        returns[i], advantages[i] = target, advantage
        next_value = float(values[i])
    return returns, advantages


@dataclass
class Rollout:
    data: np.ndarray
    offsets: np.ndarray
    actions: np.ndarray
    log_probs: np.ndarray
    values: np.ndarray
    rewards: np.ndarray
    returns: np.ndarray
    advantages: np.ndarray
    screens: np.ndarray
    acts: np.ndarray
    entropy: np.ndarray
    complete: np.ndarray
    valid: np.ndarray
    episodes: list
    timing: dict
    count: int = 0

    @classmethod
    def allocate(cls, decisions, storage_mib):
        return cls(np.empty(storage_mib * 1024 * 1024, dtype=np.uint8),
                   np.zeros(decisions + 1, dtype=np.uint64), np.empty(decisions, dtype=np.int32),
                   *[np.empty(decisions, dtype=np.float32) for _ in range(5)],
                   np.empty(decisions, dtype=np.uint8), np.empty(decisions, dtype=np.uint8),
                   np.empty(decisions, dtype=np.float32), np.zeros(decisions, dtype=bool),
                   np.zeros(decisions, dtype=bool), [], {})


class Collector:
    def __init__(self, size, decision_cap, storage_mib, device, gae_lambda):
        self.batch = spire.TrainingBatch(size, 1, decision_cap, [])
        self.arrays = views(self.batch)
        self.cap, self.storage_mib = decision_cap, storage_mib
        self.device, self.gae_lambda = device, gae_lambda
        self.size = size

    @torch.no_grad()
    def collect(self, model, roots, sampling_seed, attempts, decision_budget=None):
        model.eval()
        start = time.perf_counter()
        budget = len(roots) * self.cap if decision_budget is None else decision_budget
        assert budget > 0
        rollout = Rollout.allocate(min(len(roots) * self.cap, budget), self.storage_mib)
        episodes = [dict(seed=root["seed"], root_key=root_key(root), prefix_decisions=len(root["candidates"]),
                         candidates=[], records=[], outcome=None, fault=0) for root in roots]
        generators = [policy_rng(sampling_seed, root["seed"], attempt) for root, attempt in zip(roots, attempts)]
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
                before = time.perf_counter()
                code = self.batch.store(active, rollout.data, rollout.offsets[begin:end + 1])
                assert code == 0, f"Compact rollout storage exhausted: {code:#x}; increase storage_mib"
                storage_seconds += time.perf_counter() - before
                synchronize(self.device)
                before = time.perf_counter()
                inputs = network_inputs(self.arrays, self.device, active)
                logits, value = model(inputs)
                policy, entropy = distribution(logits, inputs["candidate_mask"])
                selected = sample(policy, [generators[current[int(slot)]] for slot in active])
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
