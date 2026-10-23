from contextlib import nullcontext
from copy import deepcopy
import importlib.util
from pathlib import Path
import struct

import pytest
import torch


path = Path(__file__).resolve().parents[1] / 'tools' / 'e4_distill.py'
spec = importlib.util.spec_from_file_location('e4_distill_under_test', path)
e4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e4)
torch.set_num_threads(1)


def observation_fragment(seeds):
    records = []
    for i in range(len(seeds)):
        record = struct.pack('<HH', 1, 2) + bytes(4) + bytes(8)
        record += struct.pack('<6h', i+1, 0, 0, 0, 0, 0)
        record += bytes([3, 7]) + struct.pack('<6h', 1, 0, 0, 1, 0, 0)
        records.append(record)
    return dict(data=torch.tensor(list(b''.join(records)), dtype=torch.uint8),
        offsets=torch.arange(len(seeds)+1)*len(records[0]),
        policy_mask=torch.tensor([[True, True]+[False]*126]*len(seeds)),
        actions=torch.zeros(len(seeds), dtype=torch.int64),
        action_valid=torch.ones(len(seeds), dtype=torch.bool))


def benchmark_row(name, config, rate):
    return dict(name=name, config=e4.asdict(config), seconds=300., actual_steps=rate*300,
                actual_steps_per_second=float(rate))


def test_benchmark_selection_is_largest_complete_model_at_half_speed():
    small = benchmark_row('small', e4.ModelConfig(), 100)
    big = benchmark_row('large', e4.LARGE, 50)
    middle = benchmark_row('medium', e4.MEDIUM, 60)
    assert e4.select_benchmark([small, big]) == e4.asdict(e4.LARGE)
    big = benchmark_row('large', e4.LARGE, 49)
    assert e4.select_benchmark([small, big, middle]) == e4.asdict(e4.MEDIUM)
    middle = benchmark_row('medium', e4.MEDIUM, 49)
    assert e4.select_benchmark([small, big, middle]) is None
    with pytest.raises(AssertionError):
        e4.select_benchmark([small, dict(big, seconds=299.)])
    with pytest.raises(AssertionError):
        e4.select_benchmark([small, dict(big, config=e4.asdict(e4.MEDIUM))])


def test_larger_networks_keep_sigmoid_value_and_old_default():
    assert e4.asdict(e4.ModelConfig()) == dict(width=128, layers=4, heads=4, feedforward=256)
    fragment = observation_fragment([1000000001, 1000000002])
    inputs = e4.packed_inputs(fragment['data'], fragment['offsets'], torch.arange(2), torch.device('cpu'))
    sizes = []
    for config in (e4.ModelConfig(), e4.MEDIUM, e4.LARGE):
        model = e4.Policy(config)
        logits, values = model(inputs)
        assert logits.shape == (2, 2) and ((values > 0) & (values < 1)).all()
        sizes.append(sum(p.numel() for p in model.parameters()))
    assert sizes[0] < sizes[1] < sizes[2]


def test_legal_teacher_to_student_kl_and_value_error_have_finite_gradients():
    teacher = torch.tensor([[2., 0., 1.e10], [0., 0., -torch.inf]])
    student = torch.tensor([[0., 1., -torch.inf], [1., 0., -torch.inf]], requires_grad=True)
    legal = torch.tensor([[True, True, False], [True, False, False]])
    values = torch.tensor([.2, .5], requires_grad=True)
    kl, error = e4.distillation_errors(student, values, teacher, torch.tensor([.4, .1]), legal)
    probability = teacher[0, :2].softmax(-1)
    expected = (probability*(teacher[0, :2].log_softmax(-1)-student[0, :2].log_softmax(-1))).sum()
    assert torch.allclose(kl[0], expected) and kl[1] == 0
    assert torch.allclose(error, torch.tensor([.04, .16]))
    (kl+.5*error).mean().backward()
    assert torch.isfinite(student.grad).all() and torch.isfinite(values.grad).all()
    assert (student.grad[:, 2] == 0).all()


def test_episode_holdout_records_never_enter_training_and_teacher_is_frozen():
    held = next(seed for seed in range(1000000000, 1000001000) if e4.held_out(seed))
    train = next(seed for seed in range(1000000000, 1000001000) if not e4.held_out(seed))
    seeds = [train, held, held, train]
    fragment = observation_fragment(seeds)
    fragment['action_valid'][-1] = False
    teacher = e4.Policy(e4.ModelConfig(width=8, layers=1, heads=2, feedforward=16)).eval().requires_grad_(False)
    training, holdout = e4.teacher_records(fragment, teacher, seeds, list(range(len(seeds))), torch.device('cpu'))
    assert training['seeds'].tolist() == [train]
    assert holdout['seeds'].tolist() == [held, held]
    assert all(parameter.grad is None for parameter in teacher.parameters())
    assert not training['teacher_logits'].requires_grad
    assert len(holdout['data']) == 2*len(training['data'])
    assert not set(training['seeds'].tolist()) & set(holdout['seeds'].tolist())
    fraction = sum(e4.held_out(seed) for seed in range(1000000000, 1000010000))/10000
    assert .04 < fraction < .06


def test_distill_optimizer_resume_preserves_adam_and_rng(tmp_path, monkeypatch):
    monkeypatch.setattr(e4.dist, 'all_reduce', lambda value, **kwargs: None)

    class LocalPolicy(e4.Policy):
        def no_sync(self):
            return nullcontext()

    config = e4.ModelConfig(width=8, layers=1, heads=2, feedforward=16)
    seed = next(seed for seed in range(1000000000, 1000001000) if not e4.held_out(seed))
    teacher = e4.Policy(config).eval().requires_grad_(False)
    fragment, _ = e4.teacher_records(observation_fragment([seed]*35), teacher, [seed]*35,
                                    list(range(35)), torch.device('cpu'))
    model = LocalPolicy(config)
    optimizer = torch.optim.Adam(model.parameters(), lr=3e-4)
    rng = torch.Generator().manual_seed(1729)
    plan, count = e4.minibatch_plan(35, 1, 0, rng, torch.device('cpu'))
    assert count == 1 and sorted(plan[0].tolist()) == list(range(35))
    e4.distill_minibatch(model, optimizer, fragment, plan[0], torch.device('cpu'), 1)
    snapshot = tmp_path / 'resume.pt'
    e4.atomic_checkpoint(snapshot, dict(model=model.state_dict(), optimizer=optimizer.state_dict(),
                                      rng=rng.get_state(), data=fragment, cursor=1))
    saved = torch.load(snapshot, weights_only=False)
    expected_rng = torch.randperm(35, generator=rng)
    e4.distill_minibatch(model, optimizer, fragment, plan[0], torch.device('cpu'), 1)
    resumed = LocalPolicy(config)
    resumed.load_state_dict(saved['model'])
    resumed_optimizer = torch.optim.Adam(resumed.parameters(), lr=3e-4)
    resumed_optimizer.load_state_dict(saved['optimizer'])
    resumed_rng = torch.Generator()
    resumed_rng.set_state(saved['rng'])
    assert torch.equal(expected_rng, torch.randperm(35, generator=resumed_rng))
    e4.distill_minibatch(resumed, resumed_optimizer, saved['data'], plan[0], torch.device('cpu'), 1)
    assert saved['cursor'] == 1
    for key, value in model.state_dict().items():
        assert torch.equal(value, resumed.state_dict()[key])
    assert optimizer.state_dict()['state']


def test_budget_resume_does_not_double_charge_and_evaluation_uses_total_cap():
    state = dict(node_seconds=0., distill_node_seconds=0., report_seconds=0.)
    e4.charge_time(state, 100., 7, True)
    resumed = deepcopy(state)
    e4.charge_time(resumed, 20., 7, True)
    assert resumed['node_seconds'] == resumed['distill_node_seconds'] == 120*7/8
    e4.charge_time(resumed, 50., 7, False)
    assert resumed['node_seconds'] == 170*7/8
    assert resumed['distill_node_seconds'] == 120*7/8
    assert resumed['report_seconds'] == 120
    total_cap = 150.
    assert (total_cap-resumed['node_seconds'])*8/7 < 2
    assert e4.stage_deadline(1000., 14400., total_cap, resumed['node_seconds'], 7, float('inf')) < 1000.
    assert e4.stage_deadline(1000., 14400., 21600., 14400., 8, float('inf')) == 8080.
    assert e4.stage_deadline(1000., 14400., 21600., 14400., 8, 1500.) == 1380.


def test_empty_minibatch_does_not_touch_optimizer_and_dummy_is_valid(monkeypatch):
    monkeypatch.setattr(e4.dist, 'all_reduce', lambda value, **kwargs: None)
    fragment = e4.dummy_fragment(torch.device('cpu'))
    inputs = e4.distill_inputs(fragment, torch.tensor([0]), torch.device('cpu'))
    model = e4.Policy(e4.ModelConfig(width=8, layers=1, heads=2, feedforward=16))
    logits, values = model(inputs)
    assert logits.shape == (1, 1) and torch.isfinite(values).all()
    optimizer = torch.optim.Adam(model.parameters(), lr=3e-4)
    before = deepcopy(model.state_dict())
    e4.distill_minibatch(model, optimizer, fragment, torch.empty(0, dtype=torch.int64), torch.device('cpu'), 1)
    assert not optimizer.state_dict()['state']
    for key, value in before.items():
        assert torch.equal(value, model.state_dict()[key])


def test_frozen_holdout_contains_only_completed_episodes_and_keeps_same_data(tmp_path):
    seed = next(seed for seed in range(1000000000, 1000001000) if e4.held_out(seed))
    teacher = e4.Policy(e4.ModelConfig(width=8, layers=1, heads=2, feedforward=16)).eval().requires_grad_(False)
    _, rows = e4.teacher_records(observation_fragment([seed]*3), teacher, [seed]*3,
                                 [10, 10, 11], torch.device('cpu'))
    folder = tmp_path / 'data-rank-0'
    folder.mkdir()
    path = folder / 'heldout-0.pt'
    e4.atomic_checkpoint(path, rows)
    state = dict(heldout=[dict(path=str(path.relative_to(tmp_path)), sha256=e4.sha256(path))])
    assert e4.freeze_holdout(state, [dict(seed=seed, distill_episode_id=10)], tmp_path, 0)
    frozen = torch.load(tmp_path / state['heldout'][0]['path'], weights_only=False)
    assert frozen['episodes'].tolist() == [10, 10]
    assert state['heldout_frozen_episodes'] == [10]
    digest = state['heldout'][0]['sha256']
    e4.atomic_checkpoint(path, e4.compact_rows(rows, torch.tensor([2])))
    assert e4.sha256(tmp_path / state['heldout'][0]['path']) == digest


def test_rl_stop_uses_C180_baseline_and_three_nonimprovements():
    best = e4.BASELINE['reach'].copy()
    floor, stagnant = e4.BASELINE['average_floor'], 0
    assessment = deepcopy(e4.BASELINE)
    for _ in range(2):
        best, floor, stagnant, stop = e4.rl_stop(assessment, best, floor, stagnant)
        assert stop is None
    best, floor, stagnant, stop = e4.rl_stop(assessment, best, floor, stagnant)
    assert stagnant == 3 and stop.startswith('three_assessments')
    better = deepcopy(assessment)
    better['reach']['act2_boss'] += .02
    assert e4.rl_stop(better, best, floor, stagnant)[2:] == (0, None)
    worse = deepcopy(assessment)
    worse['average_floor'] = floor-1.00001
    assert e4.rl_stop(worse, best, floor, 0)[3] == 'average_floor_more_than_one_below_best'
