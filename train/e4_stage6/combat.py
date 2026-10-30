from copy import deepcopy
import math

import torch

from train.model import Policy
from train.e4_timing import forward_autocast


class CombatPolicy(Policy):
    def __init__(self, config):
        super().__init__(config)
        self.combat_value = deepcopy(self.value)

    @classmethod
    def from_policy(cls, policy):
        with torch.random.fork_rng(devices=[]):
            result = cls(deepcopy(policy.config))
        state = dict(policy.state_dict())
        state.update({'combat_' + name: value for name, value in state.items()
                      if name.startswith('value.')})
        result.load_state_dict(state, strict=True)
        return result

    def forward(self, observation):
        entities, pooled = self.encode(observation)
        return (self.action_logits(observation, entities, pooled),
                self.value(pooled).squeeze(-1), self.combat_value(pooled).squeeze(-1))

    def encoder_bf16_forward(self, observation):
        with forward_autocast(next(self.parameters()).device):
            entities, pooled = self.encode(observation)
        with torch.autocast(device_type=pooled.device.type, enabled=False):
            entities, pooled = entities.float(), pooled.float()
            return (self.action_logits(observation, entities, pooled),
                    self.value(pooled).squeeze(-1), self.combat_value(pooled).squeeze(-1))

    def run_policy(self, observation):
        logits, value, _ = self(observation)
        return logits, value


def retain_trials(rng):
    return torch.randperm(32, generator=rng)[:8]




@torch.no_grad()
def combat_gae(values, terminal_score, group_mean=0.):
    assert values.ndim == 1 and len(values) and bool(torch.isfinite(values).all())
    assert math.isfinite(terminal_score) and 0 <= terminal_score <= 1
    assert math.isfinite(group_mean) and 0 <= group_mean <= 1
    centered_values = values - values[0]
    advantages = torch.empty_like(values)
    next_value = values.new_zeros(())
    advantage = values.new_zeros(())
    for t in reversed(range(len(values))):
        reward = terminal_score - group_mean if t == len(values) - 1 else 0.
        delta = reward + next_value - centered_values[t]
        advantage = delta + .95 * advantage
        advantages[t] = advantage
        next_value = centered_values[t]
    return advantages, torch.full_like(values, terminal_score)


def proportional_beta(beta, target, entropy):
    assert all(math.isfinite(x) and x > 0 for x in (beta, target))
    assert math.isfinite(entropy) and entropy >= 0
    if entropy <= target * math.sqrt(beta / .5):
        return .5
    return min(.5, beta * (target / entropy) ** 2)
