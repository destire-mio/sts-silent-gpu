from dataclasses import dataclass
import time

import numpy as np
import spire
import torch

from .model import distribution, network_inputs, views
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


def update(model, optimizer, rollout, config, device, rng):
    assert config.gamma == 1 and 0 <= config.gae_lambda <= 1
    indices = np.flatnonzero(rollout.valid[:rollout.count]).astype(np.int32)
    if not len(indices):
        return dict(samples=0, minibatches=0, update_seconds=0.0)
    synchronize(device)
    start = time.perf_counter()
    advantages = rollout.advantages[indices]
    normalized = np.zeros(rollout.count, dtype=np.float32)
    normalized[indices] = (advantages - advantages.mean()) / (advantages.std() + 1e-8)
    width = min(config.minibatch_size, len(indices))
    full = spire.ObservationBatch(width)
    remainder = len(indices) % width
    tail = spire.ObservationBatch(remainder) if remainder else full
    full_views, tail_views = views(full), views(tail)
    totals, weight, minibatches = {}, 0, 0
    model.train()
    for _ in range(config.epochs):
        rng.shuffle(indices)
        for begin in range(0, len(indices), config.minibatch_size):
            selected = indices[begin:begin + config.minibatch_size]
            padded, arrays = (full, full_views) if len(selected) == width else (tail, tail_views)
            padded.pad(rollout.data, rollout.offsets[:rollout.count + 1], selected)
            inputs = network_inputs(arrays, device, slice(None))
            logits, value = model(inputs)
            loss, metrics = objective(logits, value, inputs["candidate_mask"],
                                      torch.tensor(rollout.actions[selected], device=device, dtype=torch.long),
                                      torch.tensor(rollout.log_probs[selected], device=device),
                                      torch.tensor(rollout.returns[selected], device=device),
                                      torch.tensor(normalized[selected], device=device), config)
            assert torch.isfinite(loss).item(), "Non-finite PPO loss"
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            norm = torch.nn.utils.clip_grad_norm_(model.parameters(), config.max_grad_norm, error_if_nonfinite=True)
            optimizer.step()
            metrics.update(loss=loss, grad_norm=norm)
            for name, value in metrics.items():
                totals[name] = totals.get(name, 0.0) + float(value.detach().item()) * len(selected)
            weight += len(selected)
            minibatches += 1
    synchronize(device)
    return dict(samples=len(indices), minibatches=minibatches, update_seconds=time.perf_counter() - start,
                **{name: value / weight for name, value in totals.items()})
