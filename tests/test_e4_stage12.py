from copy import deepcopy
import gzip
import json
from pathlib import Path

import pytest
import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.e4 import RoomSearch, SegmentCollector, AllocationEnded, COMBAT_PHASE, PHASE
from train.e4_stage12 import (CLSPolicy, NoSLSearch, NoSLCollector, new_cursor,
    advance_minibatch, save_rank, restore_rank)
from train.model import network_inputs, packed_inputs, distribution


def observation():
    search = RoomSearch(2, 1, torch.device('cpu'), torch.Generator().manual_seed(42), scratch_slots=0)
    collector = SegmentCollector(search, torch.Generator().manual_seed(31))
    collector.fresh([0, 1])
    return network_inputs(search.arrays, search.device, torch.tensor([0, 1]))


def test_new_observation_forward_backward():
    torch.set_num_threads(1)
    model = CLSPolicy()
    inputs = observation()
    logits, value = model(inputs)
    policy, _ = distribution(logits, inputs['candidate_mask'])
    (policy.entropy().mean() + value.mean()).backward()
    assert torch.isfinite(model.cls.grad).all() and model.cls.grad.abs().sum() > 0
    assert all(p.grad is not None and torch.isfinite(p.grad).all() for p in model.parameters())
    print('Stage12 model parameters:', sum(p.numel() for p in model.parameters()))


def test_cls_entity_permutation():
    torch.set_num_threads(1)
    model = CLSPolicy().eval()
    inputs = observation()
    permuted = {k:v.clone() for k,v in inputs.items()}
    order = torch.randperm(inputs['entity_mask'].shape[-1])
    for name in ('tags', 'ids', 'values', 'entity_mask'):
        permuted[name] = inputs[name][..., order]
    inverse = torch.argsort(order)
    refs = inputs['candidate_refs']
    permuted['candidate_refs'] = torch.where(refs > 0, inverse[(refs - 1).clamp_min(0)] + 1, refs)
    with torch.no_grad():
        _, a = model.encode(inputs)
        _, b = model.encode(permuted)
        torch.testing.assert_close(a, b, atol=2e-6, rtol=2e-5)
        for x,y in zip(model(inputs), model(permuted)):
            torch.testing.assert_close(x, y, atol=2e-6, rtol=2e-5)


def test_checkpoint_resume_partial_collection_and_optimizer(tmp_path):
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=(tmp_path / 'gloo').as_uri(), rank=0, world_size=1)
    try:
        torch.manual_seed(71)
        policy = CLSPolicy()
        model = DistributedDataParallel(policy)
        optimizer = torch.optim.Adam(model.parameters(), lr=2e-4, eps=1e-5)
        rng = torch.Generator().manual_seed(51)
        search = NoSLSearch(2, torch.device('cpu'), 67)
        collector = NoSLCollector(search, torch.Generator().manual_seed(31))
        policy.eval()
        fragment = collector.collect(policy, length=2)
        assert not fragment['imitation'].any()
        inputs = packed_inputs(fragment['data'], fragment['offsets'], torch.arange(2), torch.device('cpu'))
        inputs['candidate_mask'] &= fragment['policy_mask'][:2, :inputs['candidate_mask'].shape[1]]
        logits, _ = policy(inputs)
        categorical, _ = distribution(logits, inputs['candidate_mask'])
        torch.testing.assert_close(categorical.log_prob(fragment['actions'][:2]), fragment['logs'][:2])
        cursor = new_cursor(fragment, rng, 1)
        advance_minibatch(model, optimizer, torch.amp.GradScaler('cuda', enabled=False), cursor, rng, 0, 1)
        policy.eval()
        calls = 0
        clock = search.clock
        def interrupt():
            nonlocal calls
            calls += 1
            if calls == 2:
                raise AllocationEnded()
            clock()
        search.clock = interrupt
        with pytest.raises(AllocationEnded):
            collector.collect(policy, length=2)
        search.clock = clock
        saved_decision_entropy = search.decision_entropy.clone()
        path = tmp_path / 'rank.pt'
        save_rank(path, collector, cursor, (1, 1), rng, dict(ready=None, seconds=1.5, decisions_start=0))
        search.evidence_directory = tmp_path
        search.last_heartbeat -= 61
        (tmp_path / 'stop-requested').touch()
        with pytest.raises(AllocationEnded):
            search.clock()
        (tmp_path / 'stop-requested').unlink()
        common = deepcopy(dict(model=policy.state_dict(), optimizer=optimizer.state_dict()))
        expected = collector.collect(policy, length=2)
        expected_state = [search.batch.state_bytes(s) for s in range(2)]
        restored_policy = CLSPolicy().eval()
        restored_policy.load_state_dict(common['model'])
        restored_optimizer = torch.optim.Adam(restored_policy.parameters(), lr=2e-4, eps=1e-5)
        restored_optimizer.load_state_dict(common['optimizer'])
        resumed_search = NoSLSearch(2, torch.device('cpu'), 0)
        resumed = NoSLCollector(resumed_search, torch.Generator().manual_seed(0))
        resumed_rng = torch.Generator()
        restored_cursor, identity, round_state = restore_rank(torch.load(path, weights_only=False), resumed, torch.device('cpu'), resumed_rng)
        assert identity == (1, 1) and round_state['seconds'] == 1.5
        torch.testing.assert_close(saved_decision_entropy, resumed_search.decision_entropy, rtol=0, atol=0)
        assert restored_cursor['epoch'] == cursor['epoch']
        for key in optimizer.state_dict()['state']:
            for name, value in optimizer.state_dict()['state'][key].items():
                torch.testing.assert_close(value, restored_optimizer.state_dict()['state'][key][name], rtol=0, atol=0)
        actual = resumed.collect(restored_policy, length=2)
        for key in expected:
            torch.testing.assert_close(actual[key], expected[key], rtol=0, atol=0)
        assert expected_state == [resumed_search.batch.state_bytes(s) for s in range(2)]
        assert torch.equal(search.rng.get_state(), resumed_search.rng.get_state())
    finally:
        dist.destroy_process_group()


def test_fast_masks_equal_original_per_slot():
    torch.set_num_threads(1)
    old = RoomSearch(2, 1, torch.device('cpu'), torch.Generator().manual_seed(81), scratch_slots=0)
    fast = NoSLSearch(2, torch.device('cpu'), 81)
    with gzip.open(Path(__file__).resolve().parents[1] / 'inventory/e4-c88-stage6-combat-exam.json.gz', 'rt') as stream:
        scene = json.load(stream)['groups']['act1_boss_guardian'][0]
    slots = [0, 1]
    for search in (old, fast):
        collector = SegmentCollector(search, torch.Generator().manual_seed(31))
        collector.reset(slots, [scene['seed'], 2000000001])
        search.batch.reset_to_many([0], [scene['seed']], [scene['prefix']])
        search.provenance[0] = (scene['seed'], scene['prefix'].copy())
        search.reset_seen(slots)
    progress = old.progress_for(slots)
    assert progress[0][PHASE] == COMBAT_PHASE and progress[1][PHASE] != COMBAT_PHASE
    rng = torch.Generator().manual_seed(53)
    for _ in range(12):
        a = old.policy_masks(slots)
        b = fast.policy_masks(slots)
        assert torch.equal(a, b)
        choices = torch.multinomial(a.float(), 1, generator=rng).flatten().tolist()
        old.step(slots, choices)
        fast.step(slots, choices)
    assert [old.batch.state_bytes(s) for s in slots] == [fast.batch.state_bytes(s) for s in slots]


def test_stage12_health_uses_normalized_entropy_and_three_kl_rounds():
    from train.e4_stage12 import health_stop
    state = {}
    metrics = dict(finite=True, kl=.11, normalized_entropy=.09, entropy=.01,
                   amp_scale=8., amp_skipped_steps=0, optimizer_attempts=64)
    assert health_stop(state, metrics, 1, 100) is None
    assert health_stop(state, metrics, 0, 100) is None
    assert health_stop(state, metrics, 0, 100) == 'KL_above_0.1_for_3_updates'
    metrics['kl'] = 0.
    assert health_stop(state, metrics, 0, 100) is None and state['high_kl_streak'] == 0
    metrics['normalized_entropy'] = .029
    assert health_stop(state, metrics, 0, 100) == 'normalized_entropy_below_0.03'
    metrics['normalized_entropy'] = .5
    assert health_stop(state, metrics, 2, 100) == 'episode_fault_rate_above_1_percent'
    metrics['finite'] = False
    assert health_stop(state, metrics, 0, 100) == 'NaN_or_Inf'


def test_faults_end_only_the_affected_episode_and_preserve_probe_evidence(tmp_path):
    search = NoSLSearch(3, torch.device('cpu'), 81)
    search.evidence_directory = tmp_path
    collector = NoSLCollector(search, torch.Generator().manual_seed(31))
    collector.reset([0, 1, 2], [1000000000, 1000000001, 1000000002])
    progress = search.progress_for([0, 1, 2])
    probe = search.probe_slots[0]
    search.provenance[probe] = (1000000000, 0, 1)
    assert search.check_rows([probe], [17], [0], None) == {probe}
    assert search.failed == {0:'engine_fault:17'}
    search.record_fault(probe, 'second report of the same failed episode')
    assert sum(search.fault_counts.values()) == 1
    def duplicate_reward(before, after):
        raise AssertionError('Duplicate floor reward')
    collector.rewards[1].advance = duplicate_reward
    policy = CLSPolicy().eval()
    actions, _, _, _ = collector.choose(policy, [0, 1, 2], progress)
    rewards, terminal = collector.advance([0, 1, 2], actions, progress)
    assert rewards[:2] == [0., 0.] and terminal[:2] == [True, True]
    assert collector.seeds[:2] == [None, None] and collector.seeds[2] is not None
    assert collector.stalled == {0, 1} and sum(search.fault_counts.values()) == 2
    records = [json.loads(line) for line in next(tmp_path.glob('episode-faults-*.jsonl')).read_text().splitlines()]
    assert records[0]['seed'] == 1000000000 and records[0]['candidates'] == [1]
    assert records[1]['reason'] == 'reward_ledger: Duplicate floor reward'
    collector.reset([0, 1], [1000000003, 1000000004])
    assert not search.failed


def test_second_map_return_ends_one_episode_without_native_step(tmp_path):
    from train.e4 import MAP, SCREEN
    from train.model import SCREENS
    search = NoSLSearch(1, torch.device('cpu'), 81)
    search.evidence_directory = tmp_path
    collector = NoSLCollector(search, torch.Generator().manual_seed(31))
    collector.reset([0], [2000000001])
    with torch.no_grad():
        for _ in range(30):
            progress = search.progress_for([0])
            if progress[0][SCREEN] == MAP:
                break
            legal = search.policy_masks([0])[0].nonzero().flatten()
            search.step([0], [int(legal[0])])
        progress = search.progress_for([0])
        assert progress[0][SCREEN] == MAP, SCREENS[progress[0][SCREEN]]
        action = int((search.arrays['candidate_kinds'][0] == 7).nonzero()[0])
        search.map_returned[0] = True
        before = search.batch.state_bytes(0)
        search.step([0], [action])
    assert search.batch.state_bytes(0) == before
    assert search.failed[0] == 'Second map return on the same floor'


def test_amp_overflow_skips_adam_without_stopping_healthy_parameters(tmp_path, capsys):
    from train.e4_stage12 import finish_metrics, health_stop
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=(tmp_path / 'amp-gloo').as_uri(), rank=0, world_size=1)
    try:
        policy = CLSPolicy()
        model = DistributedDataParallel(policy)
        optimizer = torch.optim.Adam(model.parameters(), lr=2e-4, eps=1e-5)
        rng = torch.Generator().manual_seed(51)
        collector = NoSLCollector(NoSLSearch(2, torch.device('cpu'), 67), torch.Generator().manual_seed(31))
        fragment = collector.collect(policy, length=2)
        cursor = new_cursor(fragment, rng, 1)
        scaler = torch.amp.GradScaler('cpu', init_scale=8., growth_interval=1)
        advance_minibatch(model, optimizer, scaler, cursor, rng, 0, 1)
        assert scaler.get_scale() == 16. and cursor['optimizer_steps'] == 1
        assert cursor['optimizer_attempts'] == 1 and cursor['amp_skipped_steps'] == 0
        before = deepcopy(optimizer.state_dict())
        weights = deepcopy(policy.state_dict())
        hook = policy.cls.register_hook(lambda grad: grad * float('inf'))
        advance_minibatch(model, optimizer, scaler, cursor, rng, 0, 1)
        hook.remove()
        assert scaler.get_scale() == 8.
        row = finish_metrics(cursor, model, scaler)
        assert row['optimizer_steps'] == 1 and row['optimizer_attempts'] == 2 and row['amp_skipped_steps'] == 1
        assert row['finite'] and health_stop({}, row, 0, 100) is None
        for name, value in weights.items():
            torch.testing.assert_close(value, policy.state_dict()[name], rtol=0, atol=0)
        for key in before['state']:
            for name, value in before['state'][key].items():
                torch.testing.assert_close(value, optimizer.state_dict()['state'][key][name], rtol=0, atol=0)
        events = [json.loads(line) for line in capsys.readouterr().out.splitlines() if line.startswith('{')]
        event = next(e for e in events if e.get('event') == 'nonfinite_gradient')
        assert event['rank'] == 0 and event['optimizer_step_skipped']
        assert event['scale_before'] == 16. and event['scale_after'] == 8.
        row['amp_scale'] = .5
        assert health_stop({}, row, 0, 100) == 'AMP_scale_below_1'
        row['amp_scale'] = 8.
        row['amp_skipped_steps'] = 2
        assert health_stop({}, row, 0, 100) == 'AMP_skipped_more_than_half'
        cursor['finite'].fill_(False)
        assert health_stop({}, finish_metrics(cursor, model, scaler), 0, 100) == 'NaN_or_Inf'
        cursor['finite'].fill_(True)
        with torch.no_grad():
            policy.cls.fill_(float('nan'))
        assert health_stop({}, finish_metrics(cursor, model, scaler), 0, 100) == 'NaN_or_Inf'
    finally:
        dist.destroy_process_group()



@pytest.mark.parametrize("rate,kl,expected", [
    (1e-4, .021, 1e-4 / 1.5), (1e-4, .004, 1.5e-4),
    (1e-4, .02, 1e-4), (1e-4, .005, 1e-4),
    (1e-5, .03, 1e-5), (2e-4, .001, 2e-4)])
def test_kl_adaptive_learning_rate(rate, kl, expected):
    from train.e4_stage12 import next_learning_rate
    assert next_learning_rate(rate, kl) == pytest.approx(expected)


def test_dev_snapshot_contains_only_weights_and_is_not_overwritten(tmp_path):
    from tools.e4_stage12 import save_dev_weights
    model = torch.nn.Linear(2, 1)
    before = deepcopy(model.state_dict())
    name = save_dev_weights(model, tmp_path, 14 * 3600 + 20)
    assert name == "dev-14.pt"
    raw = (tmp_path / name).read_bytes()
    with torch.no_grad():
        model.weight.add_(1)
    assert save_dev_weights(model, tmp_path, 14 * 3600 + 30) == name
    assert (tmp_path / name).read_bytes() == raw
    saved = torch.load(tmp_path / name, weights_only=True)
    assert saved.keys() == before.keys()
    for key in before:
        torch.testing.assert_close(saved[key], before[key], rtol=0, atol=0)


def test_decision_entropy_categories_and_counts():
    from train.e4 import SCREEN, ROOM
    from train.model import SCREENS
    from train.e4_stage12 import decision_type, decision_entropy_summary, DECISION_TYPES
    cases = [('combat', 0, 1), ('card_reward', 0, 2), ('map', 4, 2),
             ('shop', 3, 2), ('room', 4, 2), ('event', 6, 0)]
    for expected, (screen, room, phase) in enumerate(cases):
        progress = [0] * 9
        progress[SCREEN], progress[ROOM], progress[PHASE] = SCREENS.index(screen), room, phase
        assert decision_type(progress) == expected
    totals = torch.tensor([[.5, .9, 0, .6, .4, 0], [2, 3, 0, 2, 1, 0]], dtype=torch.float64)
    summary = decision_entropy_summary(totals)
    assert summary['entropy_by_decision_type']['combat'] == .25
    assert summary['entropy_by_decision_type']['card_reward'] == pytest.approx(.3)
    assert summary['entropy_by_decision_type']['map'] is None
    assert set(summary['decision_counts_by_type']) == set(DECISION_TYPES)
