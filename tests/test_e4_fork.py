from collections import deque
from copy import deepcopy
import gzip
import io
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

import pytest
import torch

from train.e4 import (ACT, BATTLE_OVER, BOSS_ROOM, CAPACITY_METRICS, COMBAT_PHASE, COMPLETE_PHASE,
                      DEATH, FLOOR, FORK_METRICS, HP, LOOP_METRICS, PHASE, REACH, ROOM, SCREEN,
                      SEARCH_METRICS, VICTORY, AllocationEnded, RewardLedger, RoomSearch, SegmentCollector)
from train.e4_rewards import REST_OPTIONS, RewardHistory, noncombat_summary


class ScriptedBatch:
    def __init__(self, size, mode):
        self.mode = mode
        self.states = [self.root() for _ in range(size)]
        self.faults = torch.zeros(size, dtype=torch.int64)
        self.clone_starts = []

    def root(self):
        return dict(stage=0, option=None, rng=[19, 23, 29], progress=[10, 1, COMPLETE_PHASE, 0, 6, 0, 1, 0, 50])

    def state_bytes(self, slot):
        return json.dumps(self.states[slot], sort_keys=True).encode()

    def clone_many(self, sources, targets):
        for source, target in zip(sources, targets):
            self.states[target] = deepcopy(self.states[source])
            self.faults[target] = self.faults[source]
            if source < 2 and self.states[source]['stage'] == 0:
                self.clone_starts.append(self.state_bytes(target))

    def reset_to_many(self, slots, seeds, paths):
        for slot, path in zip(slots, paths):
            self.states[slot] = self.root()
            self.faults[slot] = 0
            for action in path:
                self.advance(slot, action)

    def progress_tensors(self, output):
        output.view(-1, 9)[:] = torch.tensor([s['progress'] for s in self.states])

    def step_tensors(self, selected, active):
        for slot in active.nonzero().flatten().tolist():
            self.advance(slot, int(selected[slot]))

    def advance(self, slot, action):
        state = self.states[slot]
        stage, p = state['stage'], state['progress']
        if stage == 0:
            state['option'] = action
        state['rng'][0] += 1
        state['stage'] += 1
        if stage in (0, 2, 4):
            p[PHASE], p[SCREEN], p[BATTLE_OVER] = COMPLETE_PHASE, 9, 1
            if stage == 0 and self.mode == 'event':
                p[PHASE], p[SCREEN], p[ROOM] = 0, 8, 6
            if stage == 2 and self.mode in ('death', 'victory'):
                p[SCREEN] = DEATH if self.mode == 'death' else VICTORY
                p[HP] = 0 if self.mode == 'death' else 50
            if stage == 2 and self.mode == 'mixed' and state['option'] == 1:
                p[SCREEN], p[HP] = DEATH, 0
            if stage in (2, 4) and state['option'] == 2 and self.mode in ('capacity', 'unknown'):
                self.faults[slot] = 0x43415031 if self.mode == 'capacity' else 123
        elif stage in (1, 3):
            p[FLOOR] += 1
            p[PHASE], p[SCREEN], p[BATTLE_OVER] = COMBAT_PHASE, 0, 0
            p[ROOM] = BOSS_ROOM if self.mode == 'boss' or (self.mode == 'mixed' and state['option'] == 0) else 0
        else:
            raise AssertionError('Fork continued after its two-combat horizon')


def scripted_search(mode='two', lanes=2):
    search = object.__new__(RoomSearch)
    search.lanes, search.trials, search.size = lanes, 32, 98
    search.device = torch.device('cpu')
    search.rng = torch.Generator().manual_seed(190)
    search.available = list(range(lanes, 66))
    search.contexts, search.actual_lanes = [], []
    search.metrics, search.capacity_metrics = dict.fromkeys(SEARCH_METRICS, 0), dict.fromkeys(CAPACITY_METRICS, 0)
    search.loop_metrics = dict.fromkeys(LOOP_METRICS, 0)
    search.fork_metrics = dict.fromkeys(FORK_METRICS + SEARCH_METRICS + CAPACITY_METRICS + LOOP_METRICS, 0)
    search.searched_decisions, search.event_trial_max_steps = 0, 0
    search.provenance = {s: (100 + s, []) for s in range(search.size)}
    search.seen = {s: {s} for s in range(search.size)}
    search.mask_cache, search.frame_cache, search.combat_tails = {}, {}, {}
    search.map_returned = torch.zeros(search.size, dtype=torch.bool)
    search.reward_history = [RewardHistory() for _ in range(search.size)]
    search.played_cards_capacity_fault = 0x43415031
    search.evidence_directory, search.combat_diagnostics = None, None
    search.batch = ScriptedBatch(search.size, mode)
    search.progress_buffer = torch.empty(search.size * 9, dtype=torch.int64)
    kinds = torch.zeros((search.size, 128), dtype=torch.int64)
    kinds[:, :3] = torch.tensor([3, 3, 7])
    search.arrays = dict(candidate_kinds=kinds, faults=search.batch.faults,
                         truncated=torch.zeros(search.size, dtype=torch.bool))
    search.clock = Mock()
    search.loop_keys = lambda slots: [hash(search.batch.state_bytes(s)) for s in slots]

    def frames(slots, progress):
        return {s: dict(screen=progress[s][SCREEN], floor=progress[s][FLOOR], rewards=[],
                        actions=[(3, None), (3, None), (7, None)], rest_options=[None] * 3,
                        empty_potion_slot=True, gold=0, potion_count=0, relic_count=1, deck_size=12)
                for s in slots}

    search.frames = frames
    search.policy_masks = lambda slots: torch.tensor([[True] * 3 + [False] * 125 for _ in slots])

    def predict(model, slots):
        torch.rand(len(slots), generator=search.rng)
        if not search.contexts:
            search.actual_policy_logs = [[-1.] * 128 for _ in slots]
        elif search.combat_diagnostics is not None and search.contexts[-1]['kind'] == 'combat':
            for slot in slots:
                search.contexts[-1]['entropy_count'][slot] += 1
                search.contexts[-1]['entropy_sum'][slot] += .7
        return [0] * len(slots), [.1] * len(slots), [-1.] * len(slots)

    search.predict = predict
    search.scores = Mock(side_effect=lambda model, slots, progress:
                         {s: [.1, .8, .3][search.batch.states[s]['option'] or 0] for s in slots})
    return search


def test_scripted_fork_batches_options_and_restores_source_and_rng():
    search = scripted_search()
    before = [search.batch.state_bytes(s) for s in (0, 1)]
    rng_before = search.rng.get_state().clone()
    rewards = [RewardLedger(), RewardLedger()]
    search.metrics['combat_normal_max_steps'] = 300
    search.searched_decisions = 9
    search.capacity_metrics.update(combat_trials=17, combat_played_cards_limit=1)
    search.loop_metrics.update(repeat_check_seconds=1.5, repeat_check_candidates=7)
    capacity_before, loops_before = search.capacity_metrics.copy(), search.loop_metrics.copy()
    search._combat = Mock(wraps=search._combat)
    search.predict = Mock(wraps=search.predict)
    assert search.fork(None, {0: [0, 1, 2], 1: [0, 2]}, rewards, [0, 0], [0, 0]) == {0: 1, 1: 2}
    assert [search.batch.state_bytes(s) for s in (0, 1)] == before
    assert search.batch.clone_starts == [before[0]] * 3 + [before[1]] * 2
    assert torch.equal(search.rng.get_state(), rng_before)
    assert rewards == [RewardLedger(), RewardLedger()]
    assert search.metrics['combat_normal_max_steps'] == 300 and search.searched_decisions == 9
    assert search.capacity_metrics == capacity_before and search.loop_metrics == loops_before
    assert [len(call.args[1]) for call in search._combat.call_args_list] == [5, 5]
    assert [len(call.args[1]) for call in search.predict.call_args_list] == [5, 40, 5, 40]
    metrics = search.fork_metrics
    assert metrics['forks'] == 2 and metrics['branches'] == metrics['completed_branches'] == 5
    assert metrics['combat_trials'] == 80 and metrics['combat_normal_max_steps'] == 1
    assert metrics['event_normal_max_steps'] == 5 and metrics['searched_decisions'] == 95
    assert metrics['score_gap_count'] == 2 and metrics['score_gap_sum'] == pytest.approx(.9)
    assert metrics['score_gap_max'] == pytest.approx(.7)
    assert len(search.available) == 64 and not search.contexts


@pytest.mark.parametrize('mode,trials', [('two', 48), ('boss', 24), ('death', 24), ('victory', 24)])
def test_scripted_fork_horizons_and_terminal_bootstrap(mode, trials):
    search = scripted_search(mode)
    roots = search.available[-3:]
    assert search.fork(None, {0: [0, 1, 2]}, [RewardLedger()] * 2, [0, 0], [0, 0])[0] == (0 if mode in ('death', 'victory') else 1)
    assert search.fork_metrics['combat_trials'] == trials
    assert search.fork_metrics['completed_branches'] == 3
    terminal_calls = [call for call in search.scores.call_args_list
                      if any(s in roots for s in call.args[1])
                      and all(call.args[2][s][SCREEN] in (DEATH, VICTORY) for s in call.args[1])]
    assert not terminal_calls
    expected_gap = 0. if mode in ('death', 'victory') else .7
    assert search.fork_metrics['score_gap_max'] == pytest.approx(expected_gap)


def test_scripted_fork_inherits_floor_limits_and_counts_capacity():
    search = scripted_search()
    assert search.fork(None, {0: [0, 1, 2]}, [RewardLedger()] * 2, [200, 0], [200, 0]) == {0: None}
    assert search.fork_metrics['event_over_limit'] == search.fork_metrics['capped_branches'] == 3
    assert search.fork_metrics['combat_trials'] == 0
    assert search.fork_metrics['fork_all_capped'] == 1
    search = scripted_search('capacity')
    assert search.fork(None, {0: [0, 1, 2]}, [RewardLedger()] * 2, [0, 0], [0, 0]) == {0: 1}
    assert search.fork_metrics['combat_played_cards_limit'] == 8
    assert search.fork_metrics['capped_branches'] == 1
    assert search.fork_metrics['completed_branches'] == 2
    assert search.fork_metrics['fork_all_capped'] == 0
    assert search.capacity_metrics['combat_played_cards_limit'] == 0
    search = scripted_search('unknown')
    with pytest.raises(AssertionError, match='E4 engine fault'):
        search.fork(None, {0: [2]}, [RewardLedger()] * 2, [0, 0], [0, 0])
    assert not search.contexts and len(search.available) == 64


def test_fork_score_is_original_reward_plus_undiscounted_nonterminal_value():
    search = scripted_search('mixed')
    roots = search.available[-2:]
    assert search.fork(None, {0: [0, 1]}, [RewardLedger()] * 2, [0, 0], [0, 0]) == {0: 0}
    assert search.fork_metrics['score_gap_max'] == pytest.approx(.1 + 5 / 156)
    assert search.fork_metrics['combat_trials'] == 16
    assert all(call.args[2][s][SCREEN] not in (DEATH, VICTORY)
               for call in search.scores.call_args_list for s in call.args[1] if s in roots)


def test_fork_event_continuation_is_policy_and_counts_only_selected_combats():
    search = scripted_search('event')
    search.event = Mock(side_effect=AssertionError('Fork used event SL'))
    assert search.fork(None, {0: [0, 1, 2]}, [RewardLedger()] * 2, [0, 0], [0, 0]) == {0: 1}
    search.event.assert_not_called()
    assert search.fork_metrics['combat_trials'] == 48
    assert search.fork_metrics['event_normal_max_steps'] == 5


def test_fork_allocation_exit_restores_sources_rng_counters_and_slots():
    search = scripted_search()
    collector = collector_with_lottery(search)
    before, rng_before = search.batch.state_bytes(0), search.rng.get_state().clone()
    lottery_before, pending_before = collector.fork_rng.get_state().clone(), deepcopy(collector.pending)
    metrics_before = search.metrics.copy()
    search.clock.side_effect = [None, None, AllocationEnded]
    with pytest.raises(AllocationEnded):
        collector.choose(None, [0], search.progress(), training=True)
    assert search.batch.state_bytes(0) == before and torch.equal(search.rng.get_state(), rng_before)
    assert torch.equal(collector.fork_rng.get_state(), lottery_before) and collector.pending == pending_before
    assert search.metrics == metrics_before and not search.contexts and len(search.available) == 64


def test_fork_does_not_pollute_main_combat_diagnostics():
    search = scripted_search()
    original = [dict(trials=32)]
    search.combat_diagnostics = original
    search.fork(None, {0: [0, 1]}, [RewardLedger()] * 2, [0, 0], [0, 0])
    assert search.combat_diagnostics is original and original == [dict(trials=32)]


def test_combat_diagnostics_keep_trial_count_for_multiple_parents():
    search = scripted_search()
    for slot in (0, 1):
        search.batch.states[slot].update(stage=2, option=1)
        search.batch.states[slot]['progress'][PHASE] = COMBAT_PHASE
    search.combat_diagnostics = []
    assert search._combat(None, [0, 1], 8) == {0: [0], 1: [0]}
    assert [row['trials'] for row in search.combat_diagnostics] == [8, 8]
    assert [row['decision_count'] for row in search.combat_diagnostics] == [8, 8]


def test_fork_options_only_key_choices_and_skip():
    search = scripted_search()
    progress = search.progress()
    frame = search.frames([0], progress)[0]
    frame['actions'] += [(5, None)]
    frame['rest_options'] += [None]
    search.frames = lambda slots, p: {s: frame for s in slots}
    search.policy_masks = lambda slots: torch.ones((len(slots), 128), dtype=torch.bool)
    assert search.fork_options([0], progress) == {0: [0, 1, 2]}
    for screen in (2, 7, 8, 11):
        progress[0][SCREEN] = screen
        assert search.fork_options([0], progress) == {}
    progress[0][SCREEN] = 12
    assert search.fork_options([0], progress) == {0: [0, 1, 2]}
    progress[0][SCREEN] = 10
    frame['rest_options'] = [1, 2, None, None]
    assert search.fork_options([0], progress) == {0: [0, 1]}
    assert search.fork_options([], progress) == {}


def collector_with_lottery(search):
    collector = SegmentCollector(search, torch.Generator().manual_seed(5),
                                 fork_rng=torch.Generator().manual_seed(3))
    collector.seeds = [100, 101]
    return collector


def test_collector_imitation_executes_only_selected_one_step():
    search = scripted_search()
    collector = collector_with_lottery(search)
    source = search.batch.state_bytes(0)
    actions, _, _, imitation = collector.choose(None, [0], search.progress(), training=True)
    assert actions == [1] and imitation == [True]
    expected_lottery = torch.Generator().manual_seed(3)
    torch.rand((), generator=expected_lottery)
    assert torch.equal(collector.fork_rng.get_state(), expected_lottery.get_state())
    assert search.batch.state_bytes(0) == source and collector.paths[0] == []
    collector.advance([0], actions, search.progress())
    assert collector.paths[0] == [1] and search.batch.states[0]['stage'] == 1


@pytest.mark.parametrize('capped_slots', [(0,), (0, 1)])
def test_all_capped_forks_keep_policy_actions_without_stalling(capped_slots):
    search = scripted_search()
    collector = collector_with_lottery(search)
    collector.floor_steps = [199 if s in capped_slots else 0 for s in range(2)]
    collector.noncombat_floor_steps = collector.floor_steps.copy()
    for slot in range(2):
        search.batch.states[slot]['walk'] = slot in capped_slots
    original_advance = search.batch.advance

    def advance(slot, action):
        state = search.batch.states[slot]
        if state['walk'] and state['stage'] > 0:
            state['stage'] += 1
            state['rng'][0] += 1
        else:
            original_advance(slot, action)

    search.batch.advance = advance
    before = search.progress()
    expected_policy_rng = torch.Generator().set_state(search.rng.get_state())
    expected_fork_rng = torch.Generator().set_state(collector.fork_rng.get_state())
    torch.rand(2, generator=expected_policy_rng)
    torch.rand(2, generator=expected_fork_rng)
    actions, values, logs, imitation = collector.choose(None, [0, 1], before, training=True)
    assert torch.equal(search.rng.get_state(), expected_policy_rng.get_state())
    assert torch.equal(collector.fork_rng.get_state(), expected_fork_rng.get_state())
    assert actions == [0 if s in capped_slots else 1 for s in range(2)]
    assert values == [.1, .1] and logs == [-1., -1.]
    assert imitation == [s not in capped_slots for s in range(2)]
    assert collector.stalled == set() and all(not p for p in collector.pending)
    assert search.fork_metrics['fork_all_capped'] == len(capped_slots)
    assert search.fork_metrics['capped_branches'] == 3 * len(capped_slots)
    assert search.fork_metrics['completed_branches'] == 3 * (2 - len(capped_slots))
    _, terminal = collector.advance([0, 1], actions, before)
    assert terminal == [False, False] and not collector.completed
    assert collector.paths == [[a] for a in actions]
    assert [s['stage'] for s in search.batch.states[:2]] == [1, 1]



@pytest.mark.parametrize('kind', ['combat', 'event'])
def test_capped_room_sl_still_stalls_without_counting_a_fork(kind):
    search = scripted_search()
    collector = collector_with_lottery(search)
    search.batch.states[0]['progress'][PHASE] = COMBAT_PHASE if kind == 'combat' else COMPLETE_PHASE
    search.batch.states[0]['progress'][SCREEN] = 0 if kind == 'combat' else 8
    search.plan = Mock(return_value={0: None})
    search.fork = Mock(side_effect=AssertionError('Capped room SL triggered a fork'))
    assert collector.choose(None, [0], search.progress(), training=True)[3] == [False]
    search.plan.assert_called_once_with(None, [0], kind)
    assert collector.stalled == {0} and search.fork_metrics['fork_all_capped'] == 0


def test_evaluate_never_calls_fork_or_draws_lottery():
    search = scripted_search('victory')
    search.fork_options = Mock(side_effect=AssertionError('Evaluation considered a fork'))
    search.fork = Mock(side_effect=AssertionError('Evaluation called fork'))
    collector = collector_with_lottery(search)
    before = collector.fork_rng.get_state().clone()
    assert len(collector.evaluate(None, [100])) == 1
    assert torch.equal(before, collector.fork_rng.get_state())
    search.fork_options.assert_not_called()
    search.fork.assert_not_called()


def test_checkpoint_restores_lottery_including_evaluation_state():
    collector = collector_with_lottery(scripted_search())
    torch.rand(7, generator=collector.fork_rng)
    saved = deepcopy(collector.state())
    serialized = io.BytesIO()
    torch.save(saved, serialized)
    serialized.seek(0)
    saved = torch.load(serialized, weights_only=False)
    expected = torch.rand(64, generator=collector.fork_rng) < .25
    restored = collector_with_lottery(scripted_search())
    restored.restore(saved)
    assert torch.equal(torch.rand(64, generator=restored.fork_rng) < .25, expected)
    restored.evaluation_waiting = deque([100])
    evaluation_state = deepcopy(restored.state())
    resumed = collector_with_lottery(scripted_search('victory'))
    resumed.restore(evaluation_state)
    assert torch.equal(resumed.fork_rng.get_state(), evaluation_state['fork_rng'])


def test_allocation_exit_rolls_back_lottery_policy_rng_and_pending():
    search = scripted_search()
    collector = collector_with_lottery(search)
    search.fork = Mock(side_effect=AllocationEnded)
    before = deepcopy(collector.state())
    with pytest.raises(AllocationEnded):
        collector.choose(None, [0], search.progress(), training=True)
    assert torch.equal(collector.fork_rng.get_state(), before['fork_rng'])
    assert torch.equal(search.rng.get_state(), before['search_rng'])
    assert [list(p) for p in collector.pending] == before['pending']
    search.fork = Mock(return_value={0: 2})
    assert collector.choose(None, [0], search.progress(), training=True)[0] == [2]
    search.fork.assert_called_once()


def test_actual_trajectory_noncombat_report():
    history = RewardHistory()
    frame = dict(screen=6, floor=1, rewards=[], actions=[(3, None), (7, None)],
                 rest_options=[None, None], deck_size=12)
    history.after_step(frame, dict(frame, screen=7), 1)
    assert history.counts['card_skipped'] == history.counts['card_choices'] == 1
    history.counts.update(rest_rest=3, rest_smith=1, boss_act1_arrivals=2, boss_act1_deck_size_sum=34)
    report = noncombat_summary([dict(noncombat=dict(counts=history.counts), final_deck_size=20)])
    assert report['card_skip_rate'] == 1. and report['rest_upgrade_rate'] == .25
    assert report['boss_deck_size_mean'] == dict(act1=17., act2=None, act3=None, act4=None)
    search = scripted_search('boss')
    search.step([0], [0])
    search.step([0], [0])
    assert search.reward_history[0].counts['boss_act1_arrivals'] == 1
    assert search.reward_history[0].counts['boss_act1_deck_size_sum'] == 12


def test_b_runner_restores_c163_state_and_evaluation_resume(tmp_path, monkeypatch):
    import tools.e4_continue as runner

    source_dir, output_dir = tmp_path / 'evaluation-163', tmp_path / 'B'
    source_dir.mkdir()
    original_generator = torch.Generator
    initial = collector_with_lottery(scripted_search())
    initial.fork_rng = None
    source = dict(commit='d28c060f92e38385670d70defad5a41fea3421a9', initialization=1729,
                  version=163, world_size=7, reason='before_evaluation', lanes=256, trials=32,
                  phase='evaluation', ready=None, train_state=deepcopy(initial.state()),
                  health_history=[(1, 0, {})], imitation_beta=.007, imitation_entropy_target=.8,
                  trained_seconds=13474.011901855469, model=dict(weight=torch.tensor([163.])),
                  optimizer=dict(marker=163), ppo_rng=original_generator().manual_seed(123).get_state(),
                  training_rng=dict(torch_rng=torch.get_rng_state(), cuda_rng=torch.tensor([1], dtype=torch.uint8)))
    torch.save(source, source_dir / 'rank-0.pt')
    baseline = dict(version=163, average_floor=18., reach=dict.fromkeys(REACH, .1))
    (tmp_path / 'E4-C53-163-N32-D1024-summary.json').write_text(json.dumps(baseline))
    monkeypatch.setenv('RANK', '0')
    monkeypatch.setenv('WORLD_SIZE', '7')
    monkeypatch.setenv('LOCAL_RANK', '0')
    monkeypatch.setenv('SLURM_JOB_ID', 'cpu-mock')
    monkeypatch.setattr(torch, 'Generator', lambda device=None: original_generator())
    for name in ('set_device', 'set_rng_state'):
        monkeypatch.setattr(torch.cuda, name, Mock())
    monkeypatch.setattr(torch.cuda, 'get_rng_state', Mock(return_value=source['training_rng']['cuda_rng']))
    for name in ('init_process_group', 'barrier', 'destroy_process_group'):
        monkeypatch.setattr(runner.dist, name, Mock())
    policy = Mock()
    policy.to.return_value = policy
    policy.state_dict.return_value = source['model']
    optimizer = Mock()
    optimizer.state_dict.return_value = source['optimizer']
    monkeypatch.setattr(runner, 'Policy', Mock(return_value=policy))
    monkeypatch.setattr(runner, 'DistributedDataParallel', Mock())
    monkeypatch.setattr(runner.torch.optim, 'Adam', Mock(return_value=optimizer))
    monkeypatch.setattr(runner, 'RoomSearch', Mock(side_effect=lambda *args, **kwargs: scripted_search()))
    monkeypatch.setattr(runner, 'engine_commit', Mock(return_value='cpu-mock-implementation'))
    monkeypatch.setattr(runner, 'waiting', Mock(return_value=True))
    monkeypatch.setattr(runner, 'atomic_json', Mock())
    checkpoints = []
    monkeypatch.setattr(runner, 'atomic_checkpoint', lambda path, state: checkpoints.append(deepcopy(state)))
    argv = ['e4_fork', '--source', str(source_dir), '--output', str(output_dir),
            '--seconds', '1800', '--training-seconds', '321.5']
    monkeypatch.setattr('sys.argv', argv)
    runner.main('B')
    saved = checkpoints[-1]
    assert saved['reason'] == 'yield_to_waiting_user'
    assert saved['version'] == saved['source_version'] == 163
    assert saved['trained_seconds'] == 0 and saved['next_evaluation'] == 321.5
    assert saved['source_trained_seconds'] == source['trained_seconds']
    assert saved['target_training_seconds'] == 321.5 and saved['case'] == 'B'
    assert torch.equal(saved['ppo_rng'], source['ppo_rng'])
    assert saved['imitation_beta'] == .007 and saved['imitation_entropy_target'] == .8
    assert torch.equal(saved['train_state']['search_rng'], source['train_state']['search_rng'])
    assert saved['train_state']['fork_rng'] is not None
    policy.load_state_dict.assert_called_with(source['model'])
    optimizer.load_state_dict.assert_called_with(source['optimizer'])
    resume_dir = tmp_path / 'resume'
    resume_dir.mkdir()
    saved['fork_metrics']['fork_all_capped'] = 7
    saved['phase'] = 'evaluation'
    saved['evaluation_state'] = deepcopy(saved['train_state'])
    saved['evaluation_state']['evaluation_waiting'] = deque([2000000000])
    torch.save(saved, resume_dir / 'rank-0.pt')
    monkeypatch.setattr('sys.argv', argv + ['--resume', str(resume_dir)])
    runner.main('B')
    assert checkpoints[-1]['phase'] == 'evaluation'
    assert checkpoints[-1]['fork_metrics']['fork_all_capped'] == 7
    assert torch.equal(checkpoints[-1]['evaluation_state']['fork_rng'], saved['evaluation_state']['fork_rng'])

    resumed = checkpoints[-1]
    resumed['evaluation_state']['evaluation_waiting'] = None
    resumed['combat_diagnostics'] = []
    torch.save(resumed, resume_dir / 'rank-0.pt')
    round_name = 'E4-C68-B-163-N32-D1024'
    episode = dict(floor=18, reach=dict.fromkeys(REACH, False), outcome='death',
                   candidates=[], noncombat=dict(counts={}), final_deck_size=12)
    for rank in range(1, 7):
        rows = [dict(episode, seed=seed) for seed in range(2000000000 + rank, 2000001024, 7)]
        (output_dir / f'{round_name}-rank-{rank}.json').write_text(json.dumps(rows))
        (output_dir / f'{round_name}-combat-{rank}.json').write_text('[]')
        metrics = dict.fromkeys(resumed['fork_metrics'], 0)
        metrics['fork_all_capped'] = rank
        (output_dir / f'{round_name}-fork-{rank}.json').write_text(json.dumps(metrics))
    rows = [dict(episode, seed=seed) for seed in range(2000000000, 2000001024, 7)]
    monkeypatch.setattr(SegmentCollector, 'evaluate', Mock(return_value=rows))
    monkeypatch.setattr(runner, 'waiting', Mock(return_value=False))
    monkeypatch.setattr(runner.dist, 'all_reduce', Mock())
    monkeypatch.setattr(runner.dist, 'broadcast_object_list', Mock())
    monkeypatch.setattr(runner.shutil, 'copyfile', Mock())
    original_tensor = torch.tensor
    monkeypatch.setattr(torch, 'tensor', lambda *args, **kwargs:
                        original_tensor(*args, **dict(kwargs, device='cpu')))
    monkeypatch.setattr(runner, 'atomic_json', lambda path, value: path.write_text(json.dumps(value)))
    monkeypatch.setattr('sys.argv', argv + ['--resume', str(resume_dir), '--evaluate-only'])
    runner.main('B')
    report = json.loads((output_dir / f'{round_name}-summary.json').read_text())
    assert report['fork']['fork_all_capped'] == 7 + sum(range(1, 7))
    assert json.loads((output_dir / f'{round_name}-fork-0.json').read_text())['fork_all_capped'] == 7
    assert checkpoints[-1]['reason'] == 'evaluation_only_complete'
    assert checkpoints[-1]['fork_metrics']['fork_all_capped'] == 7
    assert checkpoints[-1]['assessments'][-1]['fork']['fork_all_capped'] == 28


def test_native_cpu_game_bytes_rng_and_selected_step(monkeypatch):
    spire = pytest.importorskip('spire', reason='Native CPU binding unavailable: Game bytes/RNG gate NOT VERIFIED')
    prefix = []
    with gzip.open(Path(__file__).parent / 'traces/full-run-24.jsonl.gz', 'rt', encoding='utf-8') as source:
        header = json.loads(next(source))
        for line in source:
            row = json.loads(line)
            if 'action' not in row:
                break
            if row['snapshot']['screen']['type'] == 'CARD_REWARD':
                break
            prefix.append(row['legal'].index(row['action']))
    assert row['snapshot']['screen']['type'] == 'CARD_REWARD', 'Native fixture has no card-reward root'

    class Policy:
        def __call__(self, inputs):
            kinds = inputs['candidate_kinds']
            return ((kinds == 1).float() * 5).masked_fill(~inputs['candidate_mask'], -torch.inf), torch.zeros(len(kinds))

    torch.set_num_threads(1)
    search = RoomSearch(1, 32, torch.device('cpu'), torch.Generator().manual_seed(68), scratch_slots=64)
    search.batch.reset_to(0, header['seed'], prefix)
    search.provenance[0] = (header['seed'], prefix.copy())
    search.reset_seen([0])
    before, starts = search.batch.state_bytes(0), []
    original_clone = search.clone

    def observe_clone(sources, targets):
        original_clone(sources, targets)
        starts.extend(search.batch.state_bytes(t) for s, t in zip(sources, targets) if s == 0)

    monkeypatch.setattr(search, 'clone', observe_clone)
    rng_before = search.rng.get_state().clone()
    options = search.fork_options([0], search.progress())
    assert options and len(options[0]) >= 2
    selected = search.fork(Policy(), options, [RewardLedger()], [0], [0])[0]
    assert selected is not None
    assert starts == [before] * len(options[0])
    assert search.batch.state_bytes(0) == before
    assert torch.equal(search.rng.get_state(), rng_before)
    search.step([0], [selected])
    replay = spire.TrainingBatch(1, 1, 2**63 - 1, [])
    replay.reset_to(0, header['seed'], prefix + [selected])
    assert search.batch.state_bytes(0) == replay.state_bytes(0)
