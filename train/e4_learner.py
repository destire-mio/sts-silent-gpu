from contextlib import nullcontext

import torch
import torch.distributed as dist

from .model import distribution, packed_inputs
from .ppo import PPOConfig


def update(model, optimizer, fragment, rng, epochs):
    config = PPOConfig(epochs=epochs, minibatch_size=8192, microbatch_size=32, gae_lambda=.99)
    world = dist.get_world_size()
    count = len(fragment["actions"])
    device = fragment["actions"].device
    ppo = ~fragment["imitation"] & fragment["action_valid"]
    returns = fragment["returns"][ppo].double()
    errors = returns - fragment["values"][ppo].double()
    value_moments = torch.stack((returns.sum(), returns.square().sum(),
                                 errors.sum(), errors.square().sum(), ppo.sum().double()))
    dist.all_reduce(value_moments)
    value_count = int(value_moments[4])
    return_variance = error_variance = explained_variance = None
    if value_count:
        return_variance = float((value_moments[1] / value_count
                                - (value_moments[0] / value_count).square()).clamp_min(0))
        error_variance = float((value_moments[3] / value_count
                               - (value_moments[2] / value_count).square()).clamp_min(0))
        if return_variance > 0:
            explained_variance = 1 - error_variance / return_variance
    advantages = fragment["advantages"]
    moments = torch.stack((advantages[ppo].double().sum(), advantages[ppo].double().square().sum(),
                           ppo.sum().double()))
    dist.all_reduce(moments)
    mean = moments[0] / moments[2].clamp_min(1)
    std = (moments[1] / moments[2].clamp_min(1) - mean.square()).clamp_min(0).sqrt()
    normalized = ((advantages - mean) / (std + 1e-8)).float()
    per_rank = config.minibatch_size // world
    total = torch.zeros(11, device=device, dtype=torch.float64)
    for _ in range(config.epochs):
        indices = torch.randperm(count, generator=rng, device=device)
        for selected in indices.split(per_rank):
            chunks = list(selected.split(config.microbatch_size))
            optimizer.zero_grad(set_to_none=True)
            for i, chunk in enumerate(chunks):
                with model.no_sync() if i + 1 < len(chunks) else nullcontext():
                    inputs = packed_inputs(fragment["data"], fragment["offsets"], chunk, device)
                    inputs["candidate_mask"] &= fragment["policy_mask"][chunk, :inputs["candidate_mask"].shape[1]]
                    logits, value = model(inputs)
                    policy, entropy = distribution(logits, inputs["candidate_mask"])
                    log_prob = policy.log_prob(fragment["actions"][chunk])
                    log_ratio = log_prob - fragment["logs"][chunk]
                    ratio = log_ratio.exp()
                    clipped = torch.minimum(ratio * normalized[chunk],
                        ratio.clamp(1 - config.clip, 1 + config.clip) * normalized[chunk])
                    policy_loss = torch.where(fragment["imitation"][chunk], -log_prob, -clipped) * fragment["action_valid"][chunk]
                    value_loss = (value - fragment["returns"][chunk]).square()
                    loss = (policy_loss + config.value_coefficient * value_loss
                            - config.entropy_coefficient * entropy * ppo[chunk]).mean()
                    assert torch.isfinite(loss), "Non-finite E4 loss"
                    (loss * len(chunk) / len(selected)).backward()
                    total += torch.stack((policy_loss.detach().double().sum(), value_loss.detach().double().sum(),
                                          fragment["imitation"][chunk].double().sum(), loss.detach().double() * len(chunk),
                                          ((ratio - 1 - log_ratio).detach() * ppo[chunk]).double().sum(),
                                          (((ratio - 1).abs() > config.clip) * ppo[chunk]).double().sum(),
                                          (entropy.detach() * ppo[chunk]).double().sum(),
                                          (-log_prob.detach() * fragment["imitation"][chunk] * fragment["action_valid"][chunk]).double().sum(),
                                          (-clipped.detach() * ppo[chunk]).double().sum(),
                                          ppo[chunk].double().sum(),
                                          (fragment["imitation"][chunk] & fragment["action_valid"][chunk]).double().sum()))
            torch.nn.utils.clip_grad_norm_(model.parameters(), config.max_grad_norm, error_if_nonfinite=True)
            optimizer.step()
    dist.all_reduce(total)
    result = dict(zip(("policy_loss", "value_loss", "imitation_fraction", "loss"),
                      (total[:4] / (count * world * config.epochs)).cpu().tolist()))
    result.update(dict(zip(("kl", "clip_fraction", "entropy", "ppo_loss"),
                           (total[[4, 5, 6, 8]] / total[9].clamp_min(1)).cpu().tolist())))
    result["imitation_loss"] = float(total[7] / total[10].clamp_min(1))
    result["ppo_samples"] = int(total[9])
    result["imitation_samples"] = int(total[10])
    result["epochs"] = epochs
    result.update(value_explained_variance=explained_variance,
                  ppo_return_variance=return_variance, ppo_value_error_variance=error_variance,
                  value_explained_variance_samples=value_count,
                  value_explained_variance_scope="pre_update_rollout_values_valid_PPO_rows_once")
    return result
