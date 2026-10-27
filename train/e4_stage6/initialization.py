from copy import deepcopy

import torch

from train.model import ModelConfig
from train.model import Policy
from train.e4_stage6.copy_optimizer import add_combat_head


def hot_policy(source, arm, device):
    assert arm in ('A', 'B')
    assert source['commit'] == '67735d2808018721eb318d2c954de6954eeb314a'
    assert source['task'] == 'rl' and source['world_size'] == 8
    assert source['state']['phase'] == 'evaluation' and source['state']['updates'] == 8
    assert source['state']['imitation_beta'] == .031673695195944346
    assert source['state']['imitation_entropy_target'] == .6716025225602738
    groups = source['optimizer']['param_groups']
    assert len(groups) == 1 and groups[0]['lr'] == 1e-4
    assert len(source['optimizer']['state']) == len(groups[0]['params']) > 0
    assert all(int(value['step']) == 512 for value in source['optimizer']['state'].values())
    policy = Policy(ModelConfig(**source['config'])).to(device)
    policy.load_state_dict(source['model'], strict=True)
    optimizer = torch.optim.Adam(policy.parameters(), lr=1e-4)
    optimizer.load_state_dict(deepcopy(source['optimizer']))
    if arm == 'B':
        policy, optimizer = add_combat_head(policy, optimizer)
    return policy, optimizer
