from copy import deepcopy

import torch

from train.e4_stage6.combat import CombatPolicy


def add_combat_head(policy, optimizer):
    assert len(optimizer.param_groups) == 1
    original_parameters = list(policy.parameters())
    assert len(optimizer.param_groups[0]['params']) == len(original_parameters)
    assert all(a is b for a, b in zip(optimizer.param_groups[0]['params'], original_parameters))
    revised = CombatPolicy.from_policy(policy).to(original_parameters[0].device)
    result = torch.optim.Adam(revised.parameters())
    source = optimizer.state_dict()
    target_ids = result.state_dict()['param_groups'][0]['params']
    source_ids = source['param_groups'][0]['params']
    mapping = dict(zip(source_ids, target_ids[:len(source_ids)]))
    group = deepcopy(source['param_groups'][0])
    group['params'] = target_ids
    result.load_state_dict(dict(state={mapping[key]: deepcopy(value) for key, value in source['state'].items()},
                                param_groups=[group]))
    return revised, result
