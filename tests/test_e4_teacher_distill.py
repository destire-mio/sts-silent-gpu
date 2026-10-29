from copy import deepcopy
import struct
from types import SimpleNamespace

import pytest
import torch

from train.e4 import COMBAT_PHASE, PHASE, ROOM, ACT, FLOOR, ELITE_ROOM, BOSS_ROOM, AllocationEnded
from train.e4_teacher_distill import (DistillationCollector, teacher_slots, split_encounters,
                              StudentEvaluationCollector, distillation_loss, fit,
                              examination_gate, training_seed, row_inputs)
from train.e4_stage6.combat import CombatPolicy
from train.model import ModelConfig
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_teacher_pilot import GROUPS


def progress(phase, room):
    row = [0] * 9
    row[PHASE], row[ROOM], row[ACT], row[FLOOR] = phase, room, 1, 6
    return row


def test_teacher_routes_only_real_elite_and_boss_combat():
    p = [progress(COMBAT_PHASE, ELITE_ROOM), progress(COMBAT_PHASE, BOSS_ROOM),
         progress(COMBAT_PHASE, 0), progress(0, ELITE_ROOM)]
    assert teacher_slots(list(range(4)), p) == [0, 1]


def test_B2_noncombat_collection_uses_frozen_inference_without_event_exploration():
    search = SimpleNamespace(lanes=1, size=129, device=torch.device('cpu'), training=True,
        actual_policy_logs=[[0., -.5]],
        predict=lambda model, slots: ([1], [0.], [-.5]))
    collector = DistillationCollector(search, torch.Generator(), torch.Generator())
    collector.seeds = [1000000000]
    search.provenance = {0: (1000000000, [])}
    choices = collector.choose(SimpleNamespace(run_policy='B2'), [0], [progress(0, 3)])
    assert choices == ([1], [0.], [-.5], [False])
    assert search.training is False


def test_training_seed_shards_are_disjoint_and_never_reach_exam_range():
    batches = []
    for rank in range(7):
        c = DistillationCollector(SimpleNamespace(lanes=2, size=258, device=torch.device('cpu')),
            torch.Generator(), torch.Generator(), rank=rank, world=7)
        c.reset = lambda slots, seeds: batches.extend(seeds)
        c.fresh([0, 1])
        c.fresh([0])
    assert len(batches) == len(set(batches)) == 21
    assert sorted(batches) == list(range(1000000000, 1000000021))


def test_encounter_split_has_no_decision_leak_and_rejects_exam_seeds():
    rows = [dict(seed=1000000000 + i, encounter=(1000000000 + i, 1, 6, ELITE_ROOM),
                 decision=j) for i in range(30) for j in range(4)]
    train, validation = split_encounters(rows)
    assert len(train) == 108 and len(validation) == 12
    assert not {r['encounter'] for r in train} & {r['encounter'] for r in validation}
    assert {r['encounter'] for r in split_encounters(rows[::-1])[1]} == {r['encounter'] for r in validation}
    rows[0]['seed'] = 2000000000
    with pytest.raises(AssertionError, match='Examination seed'):
        split_encounters(rows)
    with pytest.raises(AssertionError, match='Examination seed'):
        training_seed(999999999)


def test_loss_is_forward_KL_with_detached_B2_and_masked_candidates():
    combat = torch.tensor([[1., -1.], [-.5, .5]], requires_grad=True)
    ordinary = torch.tensor([[.7, -.3, 100.], [-.4, .4, -100.]], requires_grad=True)
    teacher = torch.tensor([[2., 0., -99.], [0., 1., 99.]], requires_grad=True)
    legal = torch.tensor([[True, True, False]] * 2)
    loss, ce, kl = distillation_loss(combat, torch.tensor([0, 1]), ordinary, teacher, legal)
    p = teacher[:, :2].detach().softmax(-1)
    expected = (p * (p.log() - ordinary[:, :2].log_softmax(-1))).sum(-1).mean()
    assert float(kl.detach()) == pytest.approx(float(expected.detach()))
    assert float(loss.detach()) == pytest.approx(float((ce + expected).detach()))
    loss.backward()
    assert teacher.grad is None
    assert torch.equal(ordinary.grad[:, 2], torch.zeros(2))
    assert bool(torch.isfinite(ordinary.grad).all()) and bool(torch.isfinite(combat.grad).all())


def test_packed_observations_real_model_and_single_legal_KL_have_finite_student_gradients():
    torch.manual_seed(96)
    raw = struct.pack('<HH4B4h6h2B6h', 1, 2, 2, 0, 0, 0, 1, 0, 0, 0,
                      20, 10, 0, 0, 0, 0, 6, 7, 0, 0, 0, 0, 0, 0)
    data = torch.tensor(list(raw), dtype=torch.uint8)
    combat = row_inputs([dict(data=data)], torch.device('cpu'))
    ordinary = row_inputs([dict(data=data, policy_mask=torch.tensor([True, False] + [False] * 126))],
                           torch.device('cpu'))
    assert ordinary['candidate_mask'].tolist() == [[True, False]]
    base = CombatPolicy(ModelConfig(16, 1, 2, 32)).eval().requires_grad_(False)
    initial = policy_hash(base)
    student = deepcopy(base).requires_grad_(True)
    target = base(ordinary)[0]
    actions = 1 - student(combat)[0].argmax(-1)
    loss, _, kl = distillation_loss(student(combat)[0], actions, student(ordinary)[0],
                                     target, ordinary['candidate_mask'])
    loss.backward()
    gradients = [p.grad for p in student.parameters() if p.grad is not None]
    assert float(kl.detach()) == 0
    assert gradients and all(bool(torch.isfinite(g).all()) for g in gradients)
    assert any(bool((g != 0).any()) for g in gradients)
    assert policy_hash(base) == initial and all(p.grad is None for p in base.parameters())


class TinyPolicy(torch.nn.Module):
    def __init__(self):
        super().__init__()
        self.weight = torch.nn.Parameter(torch.tensor([.2, -.2]))
        self.config = dict(width=2)

    def forward(self, inputs):
        return self.weight.expand(inputs['count'], -1), None, None


def test_fit_balances_rows_stops_on_plateau_and_restores_best(monkeypatch):
    import train.e4_teacher_distill as module
    rows = [dict(seed=1000000000 + i, encounter=(1000000000 + i, 1, 6, ELITE_ROOM), action=1)
            for i in range(20)]
    calls = []
    def inputs(selected, device):
        calls.append(len(selected))
        return dict(count=len(selected), candidate_mask=torch.ones(len(selected), 2, dtype=torch.bool))
    monkeypatch.setattr(module, 'row_inputs', inputs)
    scores = iter([.1, .8, .7, .9, .7])
    monkeypatch.setattr(module, 'agreement', lambda *args: next(scores))
    policy = TinyPolicy()
    initial = policy_hash(policy)
    saved = []
    student, history = fit(policy, dict(combat=rows, noncombat=[dict(seed=r['seed']) for r in rows]),
                           torch.Generator().manual_seed(96), lambda: None,
                           lambda state: saved.append(deepcopy(state)), batch_size=8)
    assert len(history) == 2 and history[-1]['improved'] is False
    assert all(r['combat_rows'] == r['noncombat_rows'] == 18 for r in history)
    assert calls == [8, 8, 8, 8, 2, 2] * 2
    assert saved[-1]['stopped_on_plateau']
    assert torch.equal(student.weight, saved[1]['best_model']['weight'])
    assert not torch.equal(student.weight, saved[-1]['model']['weight'])
    assert policy_hash(policy) == initial and all(p.grad is None for p in policy.parameters())
    assert student.config == policy.config
    assert {k: v.shape for k, v in student.state_dict().items()} == {k: v.shape for k, v in policy.state_dict().items()}


def test_collector_commits_exact_executed_T2_labels_and_limits_last_batch(monkeypatch):
    import train.e4_teacher_distill as module
    p = [progress(COMBAT_PHASE, ELITE_ROOM), progress(0, 3), progress(COMBAT_PHASE, BOSS_ROOM)]
    search = SimpleNamespace(lanes=3, size=387, device=torch.device('cpu'),
                             provenance={i: (1000000000 + i, []) for i in range(3)},
                             clock=lambda: None, progress=lambda: p,
                             policy_masks=lambda slots: torch.ones(len(slots), 128, dtype=torch.bool))
    c = DistillationCollector(search, torch.Generator(), torch.Generator())
    c.seeds = [1000000000 + i for i in range(3)]
    monkeypatch.setattr(module, 'capture', lambda search, slots: {s: torch.tensor([s], dtype=torch.uint8) for s in slots})
    monkeypatch.setattr(module.SegmentCollector, 'choose', lambda self, model, slots, p:
                        ([1] * len(slots), [0.] * len(slots), [0.] * len(slots), [False] * len(slots)))
    c.teacher.select = lambda model, slots: ([2] * len(slots),
        [dict(candidates=[1, 2], candidate_mean_scores=[.1, .8], selected=2) for s in slots])
    executed = []
    c.advance = lambda slots, actions, progress: executed.extend(zip(slots, actions))
    assert c.step(SimpleNamespace(run_policy=None), 1) == 1
    assert executed == [(0, 2), (1, 1)]
    assert len(c.combat_rows) == len(c.noncombat_rows) == 1
    assert c.combat_rows[0]['action'] == 2 and c.combat_rows[0]['candidate_mean_scores'] == [.1, .8]
    assert tuple(c.combat_rows[0]['encounter']) == (1000000000, 1, 6, ELITE_ROOM)
    assert c.encounters[(1000000000, 1, 6, ELITE_ROOM)]['prefix'] == []
    def interrupted(model, slots):
        raise AllocationEnded()
    c.teacher.select = interrupted
    with pytest.raises(AllocationEnded):
        c.step(SimpleNamespace(run_policy=None), 2)
    assert len(c.combat_rows) == 1 and len(executed) == 2


def test_student_potion_statistics_count_executed_discards_and_elite_exit(monkeypatch):
    import train.e4_teacher_distill as module
    before = [progress(COMBAT_PHASE, ELITE_ROOM)]
    after = [progress(0, ELITE_ROOM)]
    after[0][6], after[0][8] = 1, 20
    search = SimpleNamespace(lanes=1, arrays=dict(candidate_kinds=torch.tensor([[5]]),
        entity_counts=torch.tensor([2]), tags=torch.tensor([[[7, 7], [0, 0]]]),
        ids=torch.tensor([[[3, 1]]])), progress=lambda: after)
    c = StudentEvaluationCollector(search, torch.Generator())
    c.seeds = [2000000000]
    def advance(self, slots, actions, progress):
        self.completed.append(dict(seed=2000000000))
        return [0.], [True]
    monkeypatch.setattr(module.SegmentCollector, 'advance', advance)
    c.advance([0], [0], before)
    assert c.completed[0]['potion_discards'] == 1
    assert c.completed[0]['potions_after_elite'] == [dict(seed=2000000000, act=1, floor=6,
                                                       potions_remaining=1, won=True, escaped=False)]


def exam_rows():
    rows = []
    for g, count, deaths in zip(GROUPS, (400, 130, 130, 130, 200), (146, 62, 97, 121, 177)):
        for i in range(count):
            rows.append(dict(seed=2000000000 + len(rows), prefix=[i], group=g, death=i < deaths, score=0.))
    return rows


def test_stalled_evaluation_records_zero_potion_discards_without_a_real_action(monkeypatch):
    import train.e4_teacher_distill as module
    search = SimpleNamespace(lanes=1, progress=lambda: [progress(0, 3)])
    c = StudentEvaluationCollector(search, torch.Generator())
    monkeypatch.setattr(module.SegmentCollector, 'reset', lambda self, slots, seeds: self.seeds.__setitem__(0, seeds[0]))
    c.reset([0], [2000000000])
    c.stalled = {0}
    def advance(self, slots, actions, progress):
        self.completed.append(dict(seed=2000000000))
        return [0.], [True]
    monkeypatch.setattr(module.SegmentCollector, 'advance', advance)
    c.advance([0], [0], [progress(0, 3)])
    assert c.completed[0]['potion_discards'] == 0 and not c.after_elite


def test_paired_gate_strict_Boss_5pp_and_elite_3pp_boundary():
    baseline = exam_rows()
    student = deepcopy(baseline)
    boss_deaths = [r for r in student if r['group'] in GROUPS[1:4] and r['death']]
    for r in boss_deaths[:19]:
        r['death'] = False
    assert not examination_gate(baseline, student)['passed']
    boss_deaths[19]['death'] = False
    elite_alive = [r for r in student if r['group'] == GROUPS[0] and not r['death']]
    for r in elite_alive[:12]:
        r['death'] = True
    assert examination_gate(baseline, student)['passed']
    elite_alive[12]['death'] = True
    assert not examination_gate(baseline, student)['passed']
    student[0]['prefix'] = [-1]
    with pytest.raises(AssertionError):
        examination_gate(baseline, student)


def test_stage8_deadline_cannot_borrow_stage7_or_exceed_collection_cap(tmp_path, monkeypatch):
    import tools.e4_teacher_distill as run
    path = tmp_path / 'receipt'
    path.write_text('Submitted batch job 4300\n')
    ref = dict(path=str(path), sha256=run.file_digest(path))
    monkeypatch.setattr(run.time, 'time', lambda: 1000.)
    monkeypatch.setattr(run.time, 'monotonic', lambda: 100.)
    context = dict(format=1, phase='claude96_stage8', runtime_commit='source', operation='collect',
        receipt=dict(job_id=4300, world=7, wall_seconds=4000, at=980, stdout=ref),
        reservation=dict(job_id=4300, at=985, node_seconds=3500, parent_ledger=ref),
        release=dict(job_id=4300, at=990, budget_at=960, preflight_at=950, preflight=ref),
        allocation=dict(job_id=4300, world=7, cpus=8, wall_seconds=4000, start_at=990, end_at=4990, scontrol=ref),
        budgets=dict(claude96_stage8=dict(cap_node_seconds=6000, training_authorized=True,
            charged_node_seconds=0, reserved_node_seconds=3500,
            label_collection=dict(cap_node_seconds=3500, charged_node_seconds=0, reserved_node_seconds=3500))))
    assert run.allocation_deadline(context, '4300', 7, 'source') == 3880
    context['budgets']['claude96_stage8']['label_collection']['charged_node_seconds'] = 1
    with pytest.raises(AssertionError):
        run.allocation_deadline(context, '4300', 7, 'source')
    context['phase'] = 'claude94_stage7'
    with pytest.raises(AssertionError):
        run.allocation_deadline(context, '4300', 7, 'source')
