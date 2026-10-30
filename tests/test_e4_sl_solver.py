import gzip
import json
from pathlib import Path

import pytest
import torch

from train.e4 import RoomSearch
from train.e4_sl_solver import CombatSolver, summarize, value_calibration, examination_calibration
from train.e4_sl_evaluation import report
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.examination import load_roots
from train.e4_stage6.evaluation_worker import policy_hash
from train.model import ModelConfig


def test_native_batched_solver_replays_the_chosen_sequence_exactly():
    torch.set_num_threads(1)
    torch.manual_seed(97)
    search = RoomSearch(2, 32, torch.device('cpu'), torch.Generator().manual_seed(97),
                        scratch_slots=68, bosses_seen=511)
    exam_path = Path(__file__).resolve().parents[1] / 'inventory/e4-c88-stage6-combat-exam.json.gz'
    with gzip.open(exam_path, 'rt') as stream:
        scenes = json.load(stream)['groups']['act1_boss_guardian'][:2]
    roots = load_roots(search, scenes)
    policy = CombatPolicy(ModelConfig(16, 1, 2, 32)).eval().requires_grad_(False)
    policy.value[2].weight.zero_()
    policy.value[2].bias.zero_()
    original_hash = policy_hash(policy)
    original = search.batch
    original_roots = [original.state_bytes(s) for s in roots]
    widths = []
    class RecordedBatch:
        def __getattr__(self, name):
            return getattr(original, name)
        def determinize_slots(self, *args):
            pytest.fail('Legal saved-state replay must never determinize')
        def clone_many(self, sources, targets):
            if any(s in roots for s in sources):
                assert [original.state_bytes(s) for s in roots] == original_roots
            widths.append(len(sources))
            original.clone_many(sources, targets)
    search.batch = RecordedBatch()
    result = CombatSolver(search, policy, torch.Generator().manual_seed(97)).solve(roots)
    assert max(widths) == 64
    assert result['progress_reads'] == result['step_calls']
    assert result['predict_calls'] < result['step_calls']
    for j, (s, row) in enumerate(zip(roots, result['S'])):
        replay = RoomSearch(1, 32, torch.device('cpu'), torch.Generator().manual_seed(97),
                            scratch_slots=1, bosses_seen=511)
        load_roots(replay, [scenes[j]])
        replay.batch.step_slots([0], [row['actions'][0]])
        for action in row['actions'][1:]:
            replay.batch.step_slots([0], [action])
        assert original.state_bytes(s) == replay.batch.state_bytes(0)
        assert row['exact_replay'] and row['replay_score'] == row['score']
        assert row['score'] >= result['N32'][j]['score']
        assert 160 <= row['trials'] <= 4096
        assert row['no_improvement_rounds'] == 8 or row['trials'] == 4096
        assert 0 <= row['hp_fraction'] <= 1
        assert isinstance(row['potions_remaining'], int)
        assert search.provenance[s][1] == scenes[j]['prefix'] + row['actions']
    assert len(search.available) == 68 and len(set(search.available)) == 68
    assert policy_hash(policy) == original_hash and all(p.grad is None for p in policy.parameters())


def test_value_calibration_uses_deciles_for_concentrated_predictions_and_reports_ranges():
    predictions = [.05 + i * .005 for i in reversed(range(20))]
    report = value_calibration(predictions, predictions)
    assert report['samples'] == 20 and report['explained_variance'] == 1.
    assert len(report['bins']) == 10
    assert all(b['samples'] == 2 for b in report['bins'])
    assert report['bins'][0]['prediction_min'] == .05
    assert report['bins'][-1]['prediction_max'] == pytest.approx(.145)
    tied = value_calibration([.1] * 20, [0.] * 20)
    assert all(b['samples'] == 2 and b['prediction_min'] == b['prediction_max'] == .1 for b in tied['bins'])
    assert value_calibration([.1, .9], [.5, .5])['explained_variance'] is None


def test_solver_summary_reports_survivor_resources_and_trials():
    rows = [dict(group='boss', death=False, won=True, score=.7, trials=176,
                 hp_fraction=.6, potions_remaining=1),
            dict(group='boss', death=True, won=False, score=0., trials=160,
                 hp_fraction=0., potions_remaining=2)]
    row = summarize(rows)['boss']
    assert row['death_rate'] == .5 and row['trials_median'] == 168 and row['trials_max'] == 176
    assert row['won_mean_hp_fraction'] == .6 and row['won_mean_potions_remaining'] == 1.


def test_root_calibration_subtracts_already_earned_reward_from_real_evaluation():
    scenes = [dict(seed=1, prefix=[2, 3], act=1, floor=16),
              dict(seed=2, prefix=[1], act=2, floor=22)]
    episodes = [dict(seed=1, candidates=[2, 3, 0], reward=21 / 156),
                dict(seed=2, candidates=[1, 0], reward=35 / 156)]
    rows = [dict(values=[5 / 156]), dict(values=[8 / 156])]
    report = examination_calibration(scenes, rows, episodes)
    assert report['explained_variance'] == 1.
    assert report['additional_rollouts'] == 0 and report['source_job_id'] == 4145
    assert report['approximate'] and 'not B2 continuations' in report['approximation_note']


def test_evaluation_report_flags_lower_survivor_resources_and_retains_paired_coverage():
    group = 'act1_boss_guardian'
    scene = dict(seed=1, prefix=[0], act=1, floor=16, group=group)
    baseline = dict(seed=1, prefix=[0], group=group, actions=[0], death=False, won=True,
                    score=.5, trials=32, hp_fraction=.8, potions_remaining=2, values=[5 / 156])
    student = dict(baseline, score=.7, trials=160, hp_fraction=.6, potions_remaining=1)
    data = dict(B2=[dict(baseline, trials=1)], N32=[baseline], S=[student])
    from train.e4_teacher_pilot import GROUPS
    exam = dict(groups={g: [scene] if g == group else [] for g in GROUPS})
    batches = [dict(seconds=3., groups={g: int(g == group) for g in GROUPS})]
    result = report(data, batches, exam, [dict(seed=1, candidates=[0, 0], reward=21 / 156)])
    assert result['grouped']['S'][group]['possible_value_exploitation']
    assert result['paired']['S'][group]['scenes'] == 1
    assert result['value_calibration']['samples'] == 1
    assert result['actual_allocation_node_seconds'] is None
