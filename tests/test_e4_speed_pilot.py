from copy import deepcopy
from types import SimpleNamespace

import pytest
import torch
import torch.distributed as dist

from tools.e4_speed_pilot import load_B2_initialization, b2_worker, one_update, paired_summary, pilot_seeds, restore_B2_for_evaluation
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.copy_optimizer import add_combat_head
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_timing import forward_autocast, timing
from train.model import ModelConfig, Policy


def test_paired256_uses_existing_B2_cases_and_includes_both_shop_seeds():
    seeds = pilot_seeds()
    assert len(seeds) == len(set(seeds)) == 256
    assert {2000000402, 2000000879} <= set(seeds) < set(range(2000000000, 2000001024))
    baseline = [dict(seed=s, floor=20) for s in range(2000000000, 2000001024)]
    rows = [dict(seed=s, floor=19 + 2 * (i % 2), outcome='death',
                 noncombat=dict(counts={'out_of_combat_potion_discards_with_repeat_blocks': i % 2}))
            for i, s in enumerate(seeds)]
    report = paired_summary(rows, baseline, complete=True)
    assert report['paired_floor_delta'] == 0 and report['mean_floor'] == 20
    assert report['paired_floor_SE'] == pytest.approx(1 / (255 ** .5))
    assert report['no_regression_beyond_one_paired_SE']
    assert report['potion_discard_frames_with_repeat_blocks'] == 128
    with pytest.raises(AssertionError):
        paired_summary(rows[:-1], baseline, complete=True)
    with pytest.raises(AssertionError):
        paired_summary(rows, baseline + baseline[:1], complete=True)


def test_B2_initialization_releases_cached_rollouts_before_worker_allocation(monkeypatch):
    import weakref
    references = []
    kept = dict(format=1, binding={'arm': 'B'}, state={'updates': 7}, cursor=None,
                model={'weight': torch.ones(2)}, optimizer={'state': {}})
    def load(path, *, map_location, weights_only):
        assert path == 'B2-hot' and map_location == 'cpu' and not weights_only
        cached = torch.ones(1024)
        references.append(weakref.ref(cached))
        return dict(kept, ready={'cached': cached}, collector={'cached': cached})
    monkeypatch.setattr(torch, 'load', load)
    source = load_B2_initialization('B2-hot')
    assert set(source) == set(kept)
    assert all(source[k] is kept[k] for k in kept)
    assert references[0]() is None


def test_B2_factory_keeps_model_Adam_beta_and_discards_cached_data(tmp_path, monkeypatch):
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=(tmp_path / 'group').as_uri(), rank=0, world_size=1)
    try:
        inherited = Policy(ModelConfig(16, 1, 2, 32))
        inherited_optimizer = torch.optim.Adam(inherited.parameters(), lr=1e-4)
        for p in inherited.parameters():
            p.grad = torch.full_like(p, .1)
        inherited_optimizer.step()
        policy, optimizer = add_combat_head(inherited, inherited_optimizer)
        for p in policy.parameters():
            p.grad = torch.full_like(p, .1)
        optimizer.step()
        source = dict(format=1, binding=dict(arm='B', world=7, config=vars(policy.config)),
            state=dict(updates=7, beta=.0054, entropy_target=.6716), cursor=None,
            ready=dict(biased_cached_data=True), model=deepcopy(policy.state_dict()),
            optimizer=deepcopy(optimizer.state_dict()))
        original = deepcopy(source)
        monkeypatch.setattr('tools.e4_speed_pilot.CopySearch',
            lambda *a, **kw: SimpleNamespace(device=torch.device('cpu'), clock=lambda: None))
        monkeypatch.setattr('tools.e4_speed_pilot.CopyCollector',
            lambda search, rng, **kw: SimpleNamespace(search=search, seed_rng=rng))
        worker = b2_worker(source, policy_hash(policy), torch.device('cpu'), 'new-code', tmp_path, tmp_path/'stop')
        assert policy_hash(worker.model.module) == policy_hash(policy)
        assert worker.ready is worker.cursor is None and worker.state['updates'] == 0
        assert worker.state['beta'] == .0054 and worker.state['entropy_target'] == .6716
        assert worker.binding['source_update'] == 7 and worker.binding['source_world'] == 7
        assert set(worker.binding['source_Adam_step_counts']) == {1, 2}
        assert all(int(worker.optimizer.state[p]['step']) == (1 if name.startswith('combat_value.') else 2)
                   for name, p in worker.model.module.named_parameters())
        for actual, saved in zip(worker.optimizer.state.values(), optimizer.state.values()):
            assert all(torch.equal(actual[k], v) for k, v in saved.items())
        assert source['ready'] == original['ready']
        for k in source['model']:
            assert torch.equal(source['model'][k], original['model'][k])
    finally:
        dist.destroy_process_group()


def test_timing_stops_after_exactly_one_update_and_disables_instrumentation():
    class Worker:
        state = dict(updates=0)
        ready = cursor = None
        calls = 0
        def synchronize(self):
            pass
        def step(self, deadline):
            self.calls += 1
            if self.calls == 3:
                self.state['updates'] = 1
            return dict(performed_training=True)
    worker = Worker()
    report = one_update(worker, float('inf'))
    assert worker.calls == 3 and report['complete_update'] and not timing.enabled


def test_V100_uses_supported_FP32_and_Ampere_enables_forward_bf16(monkeypatch):
    calls = []
    monkeypatch.setattr(torch, 'autocast', lambda **kw: calls.append(kw))
    monkeypatch.setattr(torch.cuda, 'get_device_capability', lambda device: (7, 0))
    forward_autocast(torch.device('cuda', 0))
    assert not calls[-1]['enabled']
    monkeypatch.setattr(torch.cuda, 'get_device_capability', lambda device: (8, 0))
    forward_autocast(torch.device('cuda', 0))
    assert calls[-1]['enabled'] and calls[-1]['dtype'] == torch.bfloat16


def test_stage10_deadline_preserves_its_independent_1200_cap(tmp_path, monkeypatch):
    import tools.e4_teacher as run
    path = tmp_path / 'receipt'
    path.write_text('Submitted batch job 4400\n')
    ref = dict(path=str(path), sha256=run.file_digest(path))
    monkeypatch.setattr(run.time, 'time', lambda: 1000.)
    monkeypatch.setattr(run.time, 'monotonic', lambda: 100.)
    context = dict(format=1, phase='claude99_stage10', runtime_commit='source',
        receipt=dict(job_id=4400, world=8, wall_seconds=1200, at=980, stdout=ref),
        reservation=dict(job_id=4400, at=985, node_seconds=1200, parent_ledger=ref),
        release=dict(job_id=4400, at=990, budget_at=960, preflight_at=950, preflight=ref),
        allocation=dict(job_id=4400, world=8, cpus=9, wall_seconds=1200, start_at=990, end_at=2190, scontrol=ref),
        budgets=dict(claude99_stage10=dict(cap_node_seconds=1200,
            charged_node_seconds=0, reserved_node_seconds=1200)))
    assert run.allocation_deadline(context, '4400', 8, 'source') == 1080
    context['budgets']['claude99_stage10']['charged_node_seconds'] = 1
    with pytest.raises(AssertionError):
        run.allocation_deadline(context, '4400', 8, 'source')
    context['budgets']['claude99_stage10']['charged_node_seconds'] = 0
    context['phase'] = 'claude97_stage9'
    with pytest.raises(KeyError):
        run.allocation_deadline(context, '4400', 8, 'source')


def test_evaluation_reloads_original_B2_and_discards_the_timing_update(tmp_path):
    policy = CombatPolicy(ModelConfig(16, 1, 2, 32))
    original = policy_hash(policy)
    path = tmp_path / 'B2.pt'
    torch.save(dict(evaluation_only=True, model=policy.state_dict()), path)
    with torch.no_grad():
        next(policy.parameters()).add_(.1)
    policy.forward = policy.encoder_bf16_forward
    assert policy_hash(policy) != original
    assert restore_B2_for_evaluation(policy, path, original) == original
    assert policy.forward.__func__ is CombatPolicy.forward
    assert policy_hash(policy) == original and not policy.training
    assert all(not p.requires_grad and p.grad is None for p in policy.parameters())
    with pytest.raises(AssertionError):
        restore_B2_for_evaluation(policy, path, 'wrong B2 hash')
