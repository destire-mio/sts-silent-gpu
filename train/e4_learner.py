from contextlib import nullcontext

import torch
import torch.distributed as dist

from .model import distribution, packed_inputs
from .ppo import PPOConfig


def update(model, optimizer, fragment, rng):
    config = PPOConfig(epochs=1, minibatch_size=8192, microbatch_size=32, gae_lambda=.99)
    world = dist.get_world_size()
    count = len(fragment["actions"])
    device = fragment["actions"].device
    ppo = ~fragment["imitation"] & fragment["action_valid"]
    advantages = fragment["advantages"]
    moments = torch.stack((advantages[ppo].double().sum(), advantages[ppo].double().square().sum(),
                           ppo.sum().double()))
    dist.all_reduce(moments)
    mean = moments[0] / moments[2].clamp_min(1)
    std = (moments[1] / moments[2].clamp_min(1) - mean.square()).clamp_min(0).sqrt()
    normalized = ((advantages - mean) / (std + 1e-8)).float()
    indices = torch.randperm(count, generator=rng, device=device)
    per_rank = config.minibatch_size // world
    total = torch.zeros(4, device=device, dtype=torch.float64)
    for selected in indices.split(per_rank):
        chunks = list(selected.split(config.microbatch_size))
        optimizer.zero_grad(set_to_none=True)
        for i, chunk in enumerate(chunks):
            with model.no_sync() if i + 1 < len(chunks) else nullcontext():
                inputs = packed_inputs(fragment["data"], fragment["offsets"], chunk, device)
                inputs["candidate_mask"] &= ~(fragment["map_return_blocked"][chunk, None]
                                               & (inputs["candidate_kinds"] == 7))
                logits, value = model(inputs)
                policy, entropy = distribution(logits, inputs["candidate_mask"])
                log_prob = policy.log_prob(fragment["actions"][chunk])
                ratio = (log_prob - fragment["logs"][chunk]).exp()
                clipped = torch.minimum(ratio * normalized[chunk],
                    ratio.clamp(1 - config.clip, 1 + config.clip) * normalized[chunk])
                policy_loss = torch.where(fragment["imitation"][chunk], -log_prob, -clipped) * fragment["action_valid"][chunk]
                value_loss = (value - fragment["returns"][chunk]).square()
                loss = (policy_loss + config.value_coefficient * value_loss
                        - config.entropy_coefficient * entropy * ppo[chunk]).mean()
                assert torch.isfinite(loss), "Non-finite E4 loss"
                (loss * len(chunk) / len(selected)).backward()
                total += torch.stack((policy_loss.detach().double().sum(), value_loss.detach().double().sum(),
                                      fragment["imitation"][chunk].double().sum(), loss.detach().double() * len(chunk)))
        torch.nn.utils.clip_grad_norm_(model.parameters(), config.max_grad_norm, error_if_nonfinite=True)
        optimizer.step()
    dist.all_reduce(total)
    return dict(zip(("policy_loss", "value_loss", "imitation_fraction", "loss"),
                    (total / (count * world)).cpu().tolist()))
