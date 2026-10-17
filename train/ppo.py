from dataclasses import dataclass
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from .model import distribution, length_batches, packed_inputs, record_lengths
from .rollout import synchronize


@dataclass
class PPOConfig:
    learning_rate: float = 0.0003
    epochs: int = 3
    minibatch_size: int = 32
    clip: float = 0.2
    entropy_coefficient: float = 0.01
    value_coefficient: float = 0.5
    max_grad_norm: float = 0.5
    gae_lambda: float = 1.0
    gamma: float = 1.0


def objective(logits, values, legal, actions, old_log_probs, returns, advantages, config):
    policy, entropy = distribution(logits, legal)
    log_ratio = policy.log_prob(actions) - old_log_probs
    ratio = log_ratio.exp()
    policy_loss = -torch.minimum(ratio * advantages,
                                ratio.clamp(1 - config.clip, 1 + config.clip) * advantages).mean()
    value_loss = (values - returns).square().mean()
    loss = policy_loss + config.value_coefficient * value_loss - config.entropy_coefficient * entropy.mean()
    kl = (ratio - 1 - log_ratio).mean()
    return loss, dict(policy_loss=policy_loss, value_loss=value_loss, entropy=entropy.mean(), kl=kl,
                      clip_fraction=((ratio - 1).abs() > config.clip).float().mean())


def gather_rows(rows):
    if not dist.is_initialized():
        return rows
    count = torch.tensor([len(rows)], device=rows.device, dtype=torch.int64)
    counts = [torch.empty_like(count) for _ in range(dist.get_world_size())]
    dist.all_gather(counts, count)
    sizes = [int(c) for c in counts]
    padded = torch.zeros(max(sizes), rows.shape[1], device=rows.device, dtype=rows.dtype)
    padded[:len(rows)] = rows
    gathered = [torch.empty_like(padded) for _ in sizes]
    dist.all_gather(gathered, padded)
    return torch.cat([r[:n] for r, n in zip(gathered, sizes)])


def empty_inputs(device):
    return dict(tags=torch.zeros(1, 4, 1, dtype=torch.long, device=device),
                ids=torch.zeros(1, 4, 1, dtype=torch.long, device=device),
                values=torch.zeros(1, 6, 1, device=device),
                candidate_kinds=torch.ones(1, 1, dtype=torch.long, device=device),
                candidate_refs=torch.zeros(1, 3, 1, dtype=torch.long, device=device),
                entity_mask=torch.ones(1, 1, dtype=torch.bool, device=device),
                candidate_mask=torch.ones(1, 1, dtype=torch.bool, device=device))


def update(model, optimizer, rollout, config, device, rng, *, policy_version):
    assert config.gamma == 1 and 0 <= config.gae_lambda <= 1
    distributed = isinstance(model, DistributedDataParallel)
    rank, world = (dist.get_rank(), dist.get_world_size()) if distributed else (0, 1)
    version_ok = (rollout.policy_versions[:rollout.count] == policy_version).all().int()
    if distributed:
        dist.all_reduce(version_ok, op=dist.ReduceOp.MIN)
    assert bool(version_ok), "Stale policy samples"
    indices = torch.nonzero(rollout.valid[:rollout.count]).flatten()
    entities, candidates = record_lengths(rollout.data, rollout.offsets, indices)
    metadata = torch.stack((rollout.global_ids[indices], entities, candidates,
                            torch.full_like(indices, rank), indices), dim=1)
    if distributed:
        metadata = gather_rows(metadata)
    metadata = metadata[torch.argsort(metadata[:, 0])]
    if not len(metadata):
        return dict(samples=0, minibatches=0, update_seconds=0.0)
    assert len(torch.unique(metadata[:, 0])) == len(metadata), "Duplicate global sample IDs"
    synchronize(device)
    start = time.perf_counter()
    advantages = rollout.advantages[indices].double()
    moments = torch.stack((advantages.sum(), advantages.square().sum(), advantages.new_tensor(len(indices))))
    if distributed:
        dist.all_reduce(moments)
    mean = moments[0] / moments[2]
    std = (moments[1] / moments[2] - mean.square()).clamp_min(0).sqrt()
    normalized = torch.zeros(rollout.count, device=device)
    normalized[indices] = ((advantages - mean) / (std + 1e-8)).float()
    totals, weight, minibatches = {}, 0, 0
    model.train()
    for _ in range(config.epochs):
        for global_indices in length_batches(torch.arange(len(metadata), device=device), metadata[:, 1],
                                             metadata[:, 2], config.minibatch_size, rng):
            records = metadata[global_indices]
            selected = records[records[:, 3] == rank, 4]
            local_count = len(selected)
            inputs = packed_inputs(rollout.data, rollout.offsets, selected, device) if local_count else empty_inputs(device)
            optimizer.zero_grad(set_to_none=True)
            logits, value = model(inputs)
            if local_count:
                loss, metrics = objective(logits, value, inputs["candidate_mask"],
                    rollout.actions[selected].long(), rollout.log_probs[selected],
                    rollout.returns[selected], normalized[selected], config)
            else:
                loss = (logits.sum() + value.sum()) * 0
                metrics = {name: loss for name in ("policy_loss", "value_loss", "entropy", "kl", "clip_fraction")}
            assert torch.isfinite(loss).item(), "Non-finite PPO loss"
            (loss * (local_count * world / len(global_indices))).backward()
            norm = torch.nn.utils.clip_grad_norm_(model.parameters(), config.max_grad_norm, error_if_nonfinite=True)
            optimizer.step()
            metrics.update(loss=loss, grad_norm=norm)
            for name, value in metrics.items():
                totals[name] = totals.get(name, 0.) + value.detach().double() * local_count
            weight += local_count
            minibatches += 1
    combined = torch.stack([*totals.values(), torch.tensor(weight, device=device, dtype=torch.float64)])
    if distributed:
        dist.all_reduce(combined)
    synchronize(device)
    values = combined.cpu().tolist()
    return dict(samples=len(metadata), minibatches=minibatches, update_seconds=time.perf_counter() - start,
                **{name: value / values[-1] for name, value in zip(totals, values[:-1])})
