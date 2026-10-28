from collections import Counter
from copy import deepcopy
from types import SimpleNamespace
from unittest.mock import Mock
import pickle

import pytest
import torch

from train import e4
from train.e4_rewards import RewardHistory


def search_at_campfire(monkeypatch, options):
    search = object.__new__(e4.RoomSearch)
    search.device = torch.device('cpu')
    search.size, search.lanes = 4, 1
    search.rng = torch.Generator().manual_seed(1)
    search.contexts, search.probe_slots = [], [1, 2, 3]
    search.mask_cache, search.frame_cache = {}, {}
    search.step0_fallback_slots = set()
    search.step0_fallback, search.step0_fallback_records = Counter(), []
    search.provenance = {0: (123456789, [2, 1, 0])}
    search.reward_history = [RewardHistory() for _ in range(4)]
    search.shop_opened, search.shop_skip_used = {0: False}, {0: False}
    search.seen, search.skip_returns = {0: {100}}, {0: set()}
    search.map_returned = torch.zeros(4, dtype=torch.bool)
    search.loop_metrics = dict.fromkeys(e4.LOOP_METRICS, 0)
    progress = torch.tensor([[12, 1, 2, 4, 10, 0, 0, 0, 50]] * 4)
    search.progress_buffer = progress.flatten().clone()
    search.progress = lambda: progress.tolist()
    search.clock, search.check = Mock(), Mock(return_value=set())
    search.loop_keys = lambda slots: [200 + s for s in slots]
    search.batch = SimpleNamespace(clone_many=Mock(), step_tensors=Mock(),
        progress_tensors=lambda target: target.copy_(progress.flatten()), reset_to_many=Mock())
    search.arrays = {key: torch.empty(0) for key in e4.views_fields}
    search.arrays['candidate_kinds'] = torch.full((4, len(options)), 3)
    search.map_return_blocked = lambda slots: torch.zeros(len(slots), dtype=torch.bool)
    monkeypatch.setattr(e4, 'network_inputs', lambda *args: dict(
        candidate_mask=torch.ones(1, len(options), dtype=torch.bool),
        candidate_kinds=torch.full((1, len(options)), 3)))
    frame = dict(screen=10, floor=12, room=4, rewards=[],
        actions=[(3, None)] * len(options), rest_options=options,
        choices=[(3, [i]) for i in range(len(options))], relics=[1, 2],
        empty_potion_slot=False, gold=99, potion_count=0, relic_count=2, deck_size=12)
    search.frames = lambda slots, progress: {s: deepcopy(frame) for s in slots}
    return search, frame


def test_only_recall_fallback_real_caller_step_and_resume(monkeypatch, capsys):
    search, frame = search_at_campfire(monkeypatch, [6])
    assert search.policy_masks([0])[0].nonzero().flatten().tolist() == [0]
    assert search.policy_masks([0])[0, 0]
    assert not search.step0_fallback and not search.step0_fallback_records
    search.step([0], [0])
    assert search.provenance[0][1] == [2, 1, 0, 0]
    assert search.step0_fallback == {'room': 1}
    record = search.step0_fallback_records[0]
    assert record['seed'] == 123456789 and record['candidates'] == [2, 1, 0]
    assert record['rest_options'] == [6] and record['relics'] == [1, 2]
    assert record['selected'] == 0 and 'step0_fallback' in capsys.readouterr().out
    collector = e4.SegmentCollector(search, torch.Generator().manual_seed(2))
    saved = pickle.loads(pickle.dumps(collector.state()))
    search.step0_fallback.clear()
    search.step0_fallback_records.clear()
    collector.restore(saved)
    assert search.step0_fallback == {'room': 1}
    assert search.step0_fallback_records == [record]
    assert not search.step0_fallback_slots


def test_restored_candidates_still_pass_repeat_filter(monkeypatch):
    search, _ = search_at_campfire(monkeypatch, [6, 6])
    search.loop_keys = lambda slots: [100 if s == 1 else 200 for s in slots]
    mask = search.policy_masks([0])[0]
    assert not mask[0] and mask[1]
    assert search.loop_metrics['repeat_check_candidates'] == 2
    assert not search.step0_fallback_records


def test_empty_before_step0_still_aborts(monkeypatch):
    search, frame = search_at_campfire(monkeypatch, [6])
    monkeypatch.setattr(search.reward_history[0], 'mask', lambda frame, legal: ([False], None))
    assert e4.step0_mask(frame, [False], False) == ([False], False)
    with pytest.raises(AssertionError, match='Reward restriction removed every action'):
        search.policy_masks([0])


def test_round0_snapshot_initializes_only_new_telemetry(monkeypatch):
    search, _ = search_at_campfire(monkeypatch, [6])
    collector = e4.SegmentCollector(search, torch.Generator().manual_seed(2))
    collector.seeds[0] = 123456789
    collector.paths[0] = [2, 1, 0]
    saved = pickle.loads(pickle.dumps(collector.state()))
    del saved['step0_fallback'], saved['step0_fallback_records']
    before_rng = saved['search_rng'].clone()
    search.step0_fallback['room'] = 99
    search.step0_fallback_records.append({'stale': True})
    collector.restore(saved)
    assert search.step0_fallback == {} and search.step0_fallback_records == []
    assert collector.paths == [[2, 1, 0]]
    assert search.provenance[0] == (123456789, [2, 1, 0])
    search.batch.reset_to_many.assert_called_once_with([0], [123456789], [[2, 1, 0]])
    assert torch.equal(search.rng.get_state(), before_rng)


def test_fallback_evidence_stops_at_twenty_actual_steps(monkeypatch):
    search, _ = search_at_campfire(monkeypatch, [6])
    for _ in range(21):
        search.policy_masks([0])
        search.step([0], [0])
    assert search.step0_fallback == {'room': 21}
    assert len(search.step0_fallback_records) == 20
    assert search.step0_fallback_records[-1]['candidates'] == [2, 1, 0] + [0] * 19


def boundary_search(monkeypatch):
    search, _ = search_at_campfire(monkeypatch, [6])
    search.metrics = dict.fromkeys(e4.SEARCH_METRICS, 0)
    search.capacity_metrics = dict.fromkeys(e4.CAPACITY_METRICS, 0)
    search.event_trial_max_steps = search.searched_decisions = 0
    search.combat_diagnostics, search.available = [], [1, 2, 3]
    search.step0_fallback['grid'] = 4
    return search


def test_training_worker_restore_keeps_missing_screen_counter(monkeypatch):
    from train.e4_stage6.training_worker import TrainingWorker
    search = boundary_search(monkeypatch)
    policy = torch.nn.Linear(1, 1)
    worker = object.__new__(TrainingWorker)
    worker.model = SimpleNamespace(module=policy, train=policy.train)
    worker.optimizer = torch.optim.Adam(policy.parameters(), lr=1e-4)
    worker.collector = e4.SegmentCollector(search, torch.Generator().manual_seed(2))
    worker.device, worker.binding = torch.device('cpu'), dict(arm='A')
    worker.ppo_rng = torch.Generator().manual_seed(3)
    worker.state, worker.ready, worker.cursor = dict(updates=0), None, None
    saved = worker.snapshot()
    assert type(saved['search']['step0_fallback']) is dict
    rng = search.rng.get_state().clone()
    model = deepcopy(policy.state_dict())
    worker.restore(saved)
    search.policy_masks([0])
    search.step([0], [0])
    assert search.step0_fallback == {'grid': 4, 'room': 1}
    assert saved['search']['step0_fallback'] == {'grid': 4}
    assert torch.equal(search.rng.get_state(), rng)
    assert all(torch.equal(value, policy.state_dict()[key]) for key, value in model.items())


def test_examination_restore_keeps_missing_screen_counter(monkeypatch):
    from train.e4_stage6.evaluation_worker import EvaluationWorker, policy_hash
    monkeypatch.setattr('train.e4_stage6.evaluation_worker.validate_examination_cursor', Mock())
    search = boundary_search(monkeypatch)
    policy = torch.nn.Linear(1, 1)
    worker = object.__new__(EvaluationWorker)
    worker.policy, worker.search = policy, search
    worker.binding = dict(arm='A', policy_sha256=policy_hash(policy))
    worker.rank, worker.world, worker.exam = 0, 1, {}
    worker.device, worker.collector = torch.device('cpu'), None
    worker.state, worker.cursor = dict(phase='examination'), {}
    worker.full_run_rng = search.rng.get_state().clone()
    saved = worker.snapshot()
    assert type(saved['search']['step0_fallback']) is dict
    worker.restore(saved)
    search.policy_masks([0])
    search.step([0], [0])
    assert search.step0_fallback == {'grid': 4, 'room': 1}
    assert saved['search']['step0_fallback'] == {'grid': 4}
