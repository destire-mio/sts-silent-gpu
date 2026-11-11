from contextlib import nullcontext
from copy import deepcopy
import gzip
import importlib.util
import json
import os
import struct
from unittest.mock import patch

import torch
from torch import nn

from train.e4_combat_distill import CombatDistillation, direct_policy_kl, load_combat_targets
from train.e4_stage12 import CLSPolicy, advance_minibatch, new_cursor
from train.model import ModelConfig, distribution, packed_inputs


class SingleRankModel(nn.Module):
    def __init__(self, policy):
        super().__init__()
        self.module = policy

    def forward(self, inputs):
        return self.module(inputs)

    def no_sync(self):
        return nullcontext()


def observation_bytes():
    return struct.pack('<HH4B4h6h2B6h', 1, 2, 2, 0, 0, 0, 1, 0, 0, 0,
                       20, 10, 0, 0, 0, 0, 6, 7, 0, 0, 0, 0, 0, 0)


def assert_bits(left, right):
    if isinstance(left, torch.Tensor):
        assert left.dtype == right.dtype and left.shape == right.shape
        assert torch.equal(left.contiguous().reshape(-1).view(torch.uint8),
                           right.contiguous().reshape(-1).view(torch.uint8))
    elif isinstance(left, dict):
        assert left.keys() == right.keys()
        for key in left:
            assert_bits(left[key], right[key])
    elif isinstance(left, (list, tuple)):
        assert len(left) == len(right)
        for a, b in zip(left, right):
            assert_bits(a, b)
    else:
        assert left == right


def test_actual_ppo_zero_weight_is_bitwise_and_positive_kl_updates_trunk():
    torch.set_num_threads(1)
    torch.manual_seed(203)
    policy = CLSPolicy(ModelConfig(16, 1, 2, 32)).eval()
    raw = observation_bytes()
    data = torch.tensor(list(raw * 4), dtype=torch.uint8)
    offsets = torch.arange(4) * len(raw)
    inputs = packed_inputs(data, offsets, torch.arange(4), torch.device('cpu'))
    with torch.no_grad():
        logits, value = policy(inputs)
        dist, _ = distribution(logits, inputs['candidate_mask'])
    actions = torch.tensor([0, 1, 0, 1])
    fragment = dict(data=data, offsets=offsets, actions=actions, logs=dist.log_prob(actions),
                    advantages=torch.tensor([.2, -.3, .4, -.5]), returns=value + .2,
                    action_valid=torch.ones(4, dtype=torch.bool), policy_mask=inputs['candidate_mask'],
                    decision_types=torch.zeros(4, dtype=torch.long))
    zero = CombatDistillation.__new__(CombatDistillation)
    zero.weight, zero.maximum_ratio, zero.targets = 0., .5, None
    zero.summary = dict(roots=1)
    active = deepcopy(zero)
    active.weight = 100.
    active.targets = [(raw, [0, 1], [4., -4.])]
    initial_rng = torch.get_rng_state()
    outcomes = []
    reference = None
    if os.environ.get('E4_TEST_PRE_COMBAT_STAGE12'):
        spec = importlib.util.spec_from_file_location('pre_combat_stage12', os.environ['E4_TEST_PRE_COMBAT_STAGE12'])
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
    with patch('train.e4_stage12.dist.all_reduce'), patch(
            'train.e4_stage12.dist.all_gather', side_effect=lambda output, count: output[0].copy_(count)):
        for combat in (None, zero, active):
            torch.set_rng_state(initial_rng)
            model = SingleRankModel(deepcopy(policy))
            optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, eps=1e-5)
            rng = torch.Generator().manual_seed(203)
            cursor = new_cursor(deepcopy(fragment), rng, 1)
            advance_minibatch(model, optimizer, torch.amp.GradScaler('cpu', enabled=False), cursor, rng,
                             0, 1, microbatch=4, value_state=dict(update=0, standalone_value_coefficient=.5),
                             combat_distillation=combat)
            outcomes.append((deepcopy(model.state_dict()), deepcopy(optimizer.state_dict()),
                             rng.get_state(), torch.get_rng_state(), deepcopy(cursor)))
        if reference is not None:
            torch.set_rng_state(initial_rng)
            model = SingleRankModel(deepcopy(policy))
            optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, eps=1e-5)
            rng = torch.Generator().manual_seed(203)
            cursor = reference.new_cursor(deepcopy(fragment), rng, 1)
            reference.advance_minibatch(model, optimizer, torch.amp.GradScaler('cpu', enabled=False),
                                       cursor, rng, 0, 1, microbatch=4,
                                       value_state=dict(update=0, standalone_value_coefficient=.5))
            assert_bits(outcomes[0][:4], (model.state_dict(), optimizer.state_dict(), rng.get_state(), torch.get_rng_state()))
    assert_bits(outcomes[0][:4], outcomes[1][:4])
    for key in ('totals', 'gradient_stats', 'optimizer_steps', 'optimizer_attempts', 'finite'):
        assert_bits(outcomes[0][4][key], outcomes[1][4][key])
    report = outcomes[2][4]['combat_distill_report']
    assert report['label_points'] == 256
    assert 0 < report['gradient_ratio_after_cap'] <= .500001
    assert report['gradient_ratio_before_cap'] > report['gradient_ratio_after_cap']
    assert outcomes[2][4]['combat_distill_applied']
    assert any(not torch.equal(outcomes[2][0][key], outcomes[0][0][key])
               for key in outcomes[0][0] if 'encoder' in key or key.endswith('cls'))
    assert outcomes[2][0].keys() == outcomes[0][0].keys()
    assert outcomes[1][4]['combat_ppo_totals'][3] > 0


def test_no_significant_evidence_preserves_full_reference_policy(tmp_path):
    (tmp_path / 'manifest.json').write_text(json.dumps(dict(format='e4_combat_labels_v1')))
    row = dict(complete=True, act=2, seed=1_000_000_000, argmax=0, options=[0, 1],
               p_ref_logits=[1., 0.], p_ref_all_options=[0, 1, 2], p_ref_all_logits=[1., 0., -1.],
               packed_observation='', estimates=[dict(action=action, k=8, delta=0., paired_se=.1)
                                                 for action in (0, 1)])
    (tmp_path / 'labels-0000000.json.gz').write_bytes(gzip.compress(json.dumps(
        dict(format='e4_combat_labels_v1', rows=[row])).encode()))
    targets, _, stats = load_combat_targets(tmp_path)
    assert targets[0][1:] == ([0, 1, 2], [1., 0., -1.])
    assert stats['z_abs_gt_1_fraction'] == 0
    logits = torch.tensor([[1., 0., -1.]], requires_grad=True)
    loss = direct_policy_kl(logits, torch.tensor([targets[0][2]]), torch.ones(1, 3, dtype=torch.bool))
    assert float(loss.detach()) == 0
    loss.backward()
    assert torch.allclose(logits.grad, torch.zeros_like(logits.grad), atol=1e-7)
