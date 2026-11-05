"""CPU/native label alignment and independently capped CE gradients for #148."""
from copy import deepcopy
import math
import torch
from train import e4_expert_iteration as expert
from train.e4_stage12 import NoSLSearch, CLSPolicy
from train.e4_midstart import MidstartCollector
from train.model import ModelConfig, packed_inputs


def test_native_expert_labels_align_after_packing(tmp_path, monkeypatch):
    torch.set_num_threads(1)
    torch.manual_seed(123)
    policy = CLSPolicy(ModelConfig(32, 1, 1, 64)).eval()
    source = MidstartCollector(NoSLSearch(1, torch.device('cpu'), 987, probe_slots=128),
        torch.Generator().manual_seed(20), rank=0)
    obj = expert.ExpertIteration(source, tmp_path, float('inf'), 0)
    # Include agreeing previews in this alignment fixture; production keeps overrides only.
    monkeypatch.setattr(expert, 'override_label', lambda r: r['preview_steps'] > 0)
    obj.collect(policy, 1, 1e6)
    fragment = obj.fragment()
    assert len(fragment['actions']) >= 32
    ids = torch.arange(len(fragment['actions']))
    inputs = packed_inputs(fragment['data'], fragment['offsets'], ids, torch.device('cpu'))
    inputs['candidate_mask'] &= fragment['policy_mask'][:, :inputs['candidate_mask'].shape[1]]
    with torch.no_grad():
        logits = policy(inputs)[0].masked_fill(~inputs['candidate_mask'], -torch.inf)
    agreement = torch.tensor([r['agreement'] for r in obj.round['records']], dtype=torch.bool)
    assert int(agreement.sum()) > 0
    assert torch.equal(logits.argmax(-1)[agreement], fragment['actions'][agreement])


def test_only_statistically_supported_overrides_are_labels():
    r = dict(preview_steps=40, selected=1, candidates=[0, 1], paired_mean=2., paired_SE=1.)
    assert expert.override_label(r)
    assert not expert.override_label(dict(r, selected=0))
    assert not expert.override_label(dict(r, paired_mean=1.))
    assert not expert.override_label(dict(r, paired_mean=.5))
    assert not expert.override_label(dict(r, preview_steps=0))


def test_ce_buffer_is_capped_against_unclipped_ppo_and_amp_scale(monkeypatch):
    monkeypatch.setattr(expert.dist, 'all_reduce', lambda t: None)
    model = torch.nn.Linear(3, 2)
    parameters = list(model.parameters())
    for p in parameters: p.grad = torch.ones_like(p) * 8
    before = [p.grad.clone() for p in parameters]
    ce = torch.cat([torch.ones(p.numel()) * 800 for p in parameters])
    cursor = {}
    scaler = type('Scaler', (), {'get_scale': lambda self: 8.})()
    expert.add_capped_expert_gradients(model, ce, scaler, cursor, 1)
    applied = torch.cat([(p.grad-g).flatten() for p,g in zip(parameters,before)])
    ppo = torch.cat([g.flatten() for g in before])
    assert torch.allclose(applied.norm(), .25*ppo.norm(), atol=1e-6)
    assert math.isclose(cursor['expert_gradient_stats'][0], 100*(8**.5), rel_tol=1e-6)
    assert math.isclose(cursor['expert_gradient_stats'][1], 8**.5, rel_tol=1e-6)
    assert abs(cursor['expert_gradient_stats'][2]-.0025) < 1e-9
    for p in parameters: p.grad.zero_()
    expert.add_capped_expert_gradients(model, torch.ones_like(ce), scaler, {}, 1)
    assert all(torch.count_nonzero(p.grad)==0 for p in parameters)


def test_v2_start_marker_does_not_reload_weights_or_reset_training_state():
    from tools.e4_stage12 import enable_expert_v2
    state = dict(update=1660, training_seconds=274000., world=7,
        claude147_dev70_rollback=dict(update=1561), learning_rate=4e-5,
        value_calibration=dict(completed_updates=100))
    before = deepcopy(state)
    assert enable_expert_v2(state)
    assert all(state[k]==v for k,v in before.items())
    marker = deepcopy(state['claude148_expert_v2'])
    state['update'] += 1
    assert not enable_expert_v2(state)
    assert state['claude148_expert_v2']==marker
