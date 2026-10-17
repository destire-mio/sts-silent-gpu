from dataclasses import dataclass, fields
import time

import torch

from .model import FIELDS, distribution, network_inputs, store_inputs, views
from .sampling import root_key, root_words, sample, uniforms


def synchronize(device):
    if device.type == "cuda":
        torch.cuda.synchronize(device)


def make_batch(size, cap, device):
    if device.type == "cuda":
        import spire_gpu
        return spire_gpu.TrainingBatch(size, cap, [])
    import spire
    assert device.type == "cpu"
    return spire.TrainingBatch(size, 1, cap, [])


@dataclass
class Rollout:
    data: torch.Tensor
    offsets: torch.Tensor
    actions: torch.Tensor
    log_probs: torch.Tensor
    values: torch.Tensor
    rewards: torch.Tensor
    returns: torch.Tensor
    advantages: torch.Tensor
    screens: torch.Tensor
    acts: torch.Tensor
    entropy: torch.Tensor
    complete: torch.Tensor
    valid: torch.Tensor
    policy_versions: torch.Tensor
    global_ids: torch.Tensor
    episodes: list
    timing: dict
    count: int = 0

    @classmethod
    def allocate(cls, decisions, storage_mib, device=torch.device("cpu")):
        decisions = min(decisions, storage_mib * 1024 * 1024 // 35)
        def zeros(dtype):
            return torch.zeros(decisions, device=device, dtype=dtype)
        return cls(torch.empty(storage_mib * 1024 * 1024, dtype=torch.uint8, device=device),
                   torch.zeros(decisions + 1, dtype=torch.int64, device=device), zeros(torch.int32),
                   *[zeros(torch.float32) for _ in range(5)], zeros(torch.uint8), zeros(torch.uint8),
                   zeros(torch.float32), zeros(torch.bool), zeros(torch.bool), zeros(torch.int64),
                   zeros(torch.int64), [], {})

    def metrics_host(self):
        return {f.name: getattr(self, f.name)[:self.count].cpu().numpy() for f in fields(self)
                if f.name not in ("data", "offsets", "episodes", "timing", "count")}


class Collector:
    def __init__(self, size, decision_cap, storage_mib, device, gae_lambda, *, batch=None):
        self.batch = make_batch(size, decision_cap, device) if batch is None else batch
        self.arrays = views(self.batch, device)
        for name in ("entity_counts", "candidate_counts", "faults", "done", "truncated", "rewards"):
            self.arrays[name] = torch.as_tensor(getattr(self.batch, name), device=device)
        self.cap, self.storage_mib = decision_cap, storage_mib
        self.device, self.gae_lambda, self.size = device, gae_lambda, size

    @torch.no_grad()
    def collect(self, model, roots, sampling_seed, attempts, decision_budget=None, *, policy_version):
        model.eval()
        synchronize(self.device)
        start = time.perf_counter()
        n = len(roots)
        budget = min(n * self.cap, n * self.cap if decision_budget is None else decision_budget)
        attempts = list(attempts)
        assert len(attempts) == n and (budget > 0 or n == 0)
        device = self.device
        rollout = Rollout.allocate(budget, self.storage_mib, device)
        def tensor(x, dtype=torch.int64):
            return torch.tensor(x, device=device, dtype=dtype)
        seeds = tensor([r["seed"] for r in roots])
        offsets = tensor([0] + [len(r["candidates"]) for r in roots]).cumsum(0)
        prefixes = tensor([c for r in roots for c in r["candidates"]] + [0], torch.int32)
        keys, attempts_tensor = root_words(roots, device), tensor(attempts)
        current = torch.full((self.size,), -1, device=device, dtype=torch.int64)
        admission = torch.zeros_like(current)
        outcomes = torch.zeros(n, device=device, dtype=torch.int64)
        faults = torch.zeros_like(outcomes)
        decisions = torch.zeros_like(outcomes)
        bootstrap = torch.zeros(n, device=device)
        record_episode = torch.empty(len(rollout.actions), device=device, dtype=torch.int64)
        record_decision = torch.empty_like(record_episode)
        next_episode = 0
        timers = {k: [] for k in ("engine_encode_seconds", "inference_seconds", "storage_seconds")}

        def timed(name, operation):
            if device.type == "cuda":
                before, after = torch.cuda.Event(enable_timing=True), torch.cuda.Event(enable_timing=True)
                before.record()
                result = operation()
                after.record()
                timers[name].append((before, after))
            else:
                before = time.perf_counter()
                result = operation()
                timers[name].append(time.perf_counter() - before)
            return result

        while rollout.count < budget:
            free = torch.nonzero(current < 0).flatten()
            count = min(len(free), n - next_episode)
            if count:
                slots = free[:count]
                assigned = torch.arange(next_episode, next_episode + count, device=device)
                current[slots], admission[slots] = assigned, assigned
                timed("engine_encode_seconds", lambda: self.batch.reset_tensors(slots, assigned, seeds, offsets, prefixes))
                next_episode += count
            failed = torch.nonzero((current >= 0) & (self.arrays["faults"].long() != 0)).flatten()
            outcomes[current[failed]] = 4
            faults[current[failed]] = self.arrays["faults"].long()[failed]
            current[failed] = -1
            slots = torch.nonzero(current >= 0).flatten()
            slots = slots[torch.argsort(admission[slots])][:budget - rollout.count]
            if not len(slots):
                if next_episode == n:
                    break
                continue
            wave_begin = rollout.count
            assert wave_begin + len(slots) <= len(rollout.actions), "Compact rollout storage exhausted; increase storage_mib"
            ids = current[slots]
            draws = timed("inference_seconds", lambda: uniforms(sampling_seed, keys[ids], attempts_tensor[ids], decisions[ids]))
            timed("storage_seconds", lambda: store_inputs(self.batch, self.arrays, slots, rollout.data,
                                                          rollout.offsets[wave_begin:wave_begin + len(slots) + 1]))
            choices = torch.zeros(self.size, device=device, dtype=torch.int32)
            mask = torch.zeros(self.size, device=device, dtype=torch.uint8)
            for active in slots.split(256):
                ids = current[active]
                begin, end = rollout.count, rollout.count + len(active)

                def infer():
                    inputs = network_inputs({k: self.arrays[k] for k in FIELDS}, device, active)
                    logits, value = model(inputs)
                    policy, entropy = distribution(logits, inputs["candidate_mask"])
                    choices = sample(policy, draws[begin - wave_begin:end - wave_begin])
                    rollout.values[begin:end] = value
                    rollout.entropy[begin:end] = entropy
                    rollout.log_probs[begin:end] = policy.log_prob(choices)
                    return choices

                selected = timed("inference_seconds", infer)
                rollout.actions[begin:end] = selected
                rollout.policy_versions[begin:end] = policy_version
                rollout.global_ids[begin:end] = attempts_tensor[ids] * self.cap + decisions[ids]
                rollout.screens[begin:end] = self.arrays["ids"][active, 0, 0] - 1
                rollout.acts[begin:end] = self.arrays["values"][active, 0, 0]
                record_episode[begin:end], record_decision[begin:end] = ids, decisions[ids]
                decisions[ids] += 1
                choices[active], mask[active] = selected, 1
                rollout.count = end
            timed("engine_encode_seconds", lambda: self.batch.step_tensors(choices, mask))
            rollout.rewards[wave_begin:rollout.count] = self.arrays["rewards"][slots]
            ids = current[slots]
            failed = self.arrays["faults"].long()[slots] != 0
            truncated = self.arrays["truncated"][slots].bool() & ~failed
            done = self.arrays["done"][slots].bool() & ~failed
            finished = failed | truncated | done
            outcomes[ids] = torch.where(failed, 4, torch.where(truncated, 3,
                torch.where(done, torch.where(self.arrays["rewards"][slots] == 1, 1, 2), 0)))
            faults[ids] = self.arrays["faults"].long()[slots]
            for tail in slots[truncated].split(256):
                if len(tail):
                    def infer_tail():
                        _, value = model(network_inputs({k: self.arrays[k] for k in FIELDS}, device, tail))
                        bootstrap[current[tail]] = value
                    timed("inference_seconds", infer_tail)
            current[slots[finished]] = -1
        live = current[current >= 0]
        outcomes[live] = 5
        self.finish_targets(rollout, record_episode, record_decision, outcomes, bootstrap)
        synchronize(device)
        outcome_names = (None, "victory", "death", "truncated", "fault", "budget_cutoff")
        host_outcomes, host_faults, host_bootstrap = outcomes.cpu().tolist(), faults.cpu().tolist(), bootstrap.cpu().tolist()
        episodes = [dict(seed=r["seed"], root_key=root_key(r), prefix_decisions=len(r["candidates"]),
                         attempt=a, policy_version=policy_version, candidates=[], records=[],
                         outcome=outcome_names[o], fault=f)
                    for r, a, o, f in zip(roots, attempts, host_outcomes, host_faults)]
        for i, (ep, choice) in enumerate(zip(record_episode[:rollout.count].cpu().tolist(),
                                            rollout.actions[:rollout.count].cpu().tolist())):
            episodes[ep]["records"].append(i)
            episodes[ep]["candidates"].append(choice)
        for i, episode in enumerate(episodes):
            if host_outcomes[i] in (1, 2, 3):
                episode["bootstrap"] = host_bootstrap[i]
        rollout.episodes = [e for e in episodes if e["outcome"] is not None]
        rollout.timing = {name: sum(a.elapsed_time(b) / 1000 for a, b in times) if device.type == "cuda"
                          else sum(times) for name, times in timers.items()}
        rollout.timing["collection_seconds"] = time.perf_counter() - start
        return rollout

    def finish_targets(self, rollout, episode, decision, outcomes, bootstrap):
        n = rollout.count
        if not n:
            return
        episode, decision = episode[:n], decision[:n]
        order = torch.argsort(episode * self.cap + decision)
        ep = episode[order]
        rewards, values = rollout.rewards[order].double(), rollout.values[order].double()
        successor = torch.cat((ep[1:] == ep[:-1], torch.zeros(1, device=self.device, dtype=torch.bool)))
        tails = torch.where(successor, torch.cat((values[1:], values.new_zeros(1))), bootstrap[ep].double())
        returns = rewards + torch.where(successor, 0., bootstrap[ep].double())
        advantages = rewards + tails - values
        step = 1
        while step < min(self.cap, n):
            same = ep[:-step] == ep[step:]
            r, a = torch.zeros_like(returns), torch.zeros_like(advantages)
            r[:-step] = torch.where(same, returns[step:], 0.)
            a[:-step] = torch.where(same, advantages[step:] * self.gae_lambda ** step, 0.)
            returns, advantages = returns + r, advantages + a
            step *= 2
        rollout.returns[order], rollout.advantages[order] = returns.float(), advantages.float()
        rollout.valid[:n] = (outcomes[episode] >= 1) & (outcomes[episode] <= 3)
        rollout.complete[:n] = (outcomes[episode] == 1) | (outcomes[episode] == 2)
