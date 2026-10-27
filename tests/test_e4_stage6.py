from copy import deepcopy
from dataclasses import asdict
from datetime import timedelta
import gzip
import json
from pathlib import Path
import struct
import time
from types import SimpleNamespace

import pytest
import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.model import ModelConfig, Policy, packed_inputs
from train.e4 import CAPACITY_METRICS, LOOP_METRICS, REACH, SEARCH_METRICS
from train.e4_stage6.combat import CombatPolicy, combat_gae, proportional_beta, retain_trials
from train.e4_stage6.copy_learner import begin_update, advance_update
from train.e4_stage6.copy_tape import CombatTape
from train.e4_stage6.initialization import hot_policy
from train.e4_stage6.phase6_controller import Phase6Schedule
from train.e4_stage6.phase6_runtime import Phase6Runtime
from train.e4_stage6.phase6_storage import Phase6Storage
from train.e4_stage6.training_worker import TrainingWorker


@pytest.fixture
def cpu_group(tmp_path):
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=(tmp_path / 'gloo').as_uri(), rank=0, world_size=1,
                            timeout=timedelta(seconds=30))
    yield
    dist.destroy_process_group()


def record():
    return torch.tensor(list(struct.pack('<HH', 1, 2) + bytes(4) + bytes(8)
        + struct.pack('<6h', 1, 0, 0, 0, 0, 0) + bytes((3, 7)) + struct.pack('<6h', 1, 0, 0, 1, 0, 0)),
        dtype=torch.uint8)


def fragment():
    data = record()
    row = dict(data=data, policy_mask=torch.tensor([True, True] + [False] * 126), action=0, log=-.7, value=.4)
    group = dict(seed=1500000000, prefix=[0], scores=torch.full((32,), .6), group_mean=.6,
                 adopted_trial=0, selected_trials=[0], trials=[dict(trial=0, adopted=True, rows=[row],
                 advantages=torch.tensor([.2]), returns=torch.tensor([.8]))])
    return dict(data=data.repeat(3), offsets=torch.arange(4) * len(data),
        actions=torch.tensor([0, 1, 0]), logs=torch.full((3,), -.7), values=torch.full((3,), .4),
        returns=torch.tensor([.5, .7, .2]), advantages=torch.tensor([.1, .3, -.2]),
        policy_mask=row['policy_mask'].repeat(3, 1), imitation=torch.tensor([False, True, False]),
        combat=torch.tensor([True, False, False]), action_valid=torch.tensor([True, True, False]),
        training_seeds=torch.full((3,), 1500000000, dtype=torch.int64), source_partition='train',
        health=torch.tensor([1, 10, 0, 0]), combat_copies=[group])


class SyntheticCollector:
    def __init__(self):
        self.seeds, self.paths = [1500000000], [[0]]
        self.seed_rng = torch.Generator().manual_seed(19)
        self.tape = CombatTape(torch.Generator().manual_seed(20))
        self.tape.groups = deepcopy(fragment()['combat_copies'])
        self.search = SimpleNamespace(device=torch.device('cpu'), lanes=1, contexts=[], clock=lambda: None,
            rng=torch.Generator().manual_seed(21), batch=SimpleNamespace(state_bytes=lambda s: json.dumps(self.paths[s]).encode()),
            metrics=dict.fromkeys(SEARCH_METRICS, 0), capacity_metrics=dict.fromkeys(CAPACITY_METRICS, 0),
            loop_metrics=dict.fromkeys(LOOP_METRICS, 0), event_trial_max_steps=0, searched_decisions=0,
            combat_diagnostics=[], available=[])

    def collect(self, policy):
        return fragment()

    def state(self):
        return dict(seeds=self.seeds.copy(), paths=deepcopy(self.paths), partial=None,
            seed_rng=self.seed_rng.get_state(), search_rng=self.search.rng.get_state(), combat_tape=self.tape.state())

    def restore(self, saved):
        self.seeds, self.paths = saved['seeds'], saved['paths']
        self.seed_rng.set_state(saved['seed_rng'])
        self.search.rng.set_state(saved['search_rng'])
        self.tape.restore(saved['combat_tape'])


def test_hot_source_keeps_512_step_Adam_independent_and_only_new_head_starts_fresh():
    torch.set_num_threads(1)
    policy = Policy(ModelConfig(16, 1, 2, 32))
    optimizer = torch.optim.Adam(policy.parameters(), lr=1e-4)
    for _ in range(512):
        optimizer.zero_grad(set_to_none=True)
        sum(p.square().sum() for p in policy.parameters()).backward()
        optimizer.step()
    source = dict(commit='67735d2808018721eb318d2c954de6954eeb314a', task='rl', world_size=8,
        config=asdict(policy.config), model=deepcopy(policy.state_dict()), optimizer=deepcopy(optimizer.state_dict()),
        state=dict(phase='evaluation', updates=8, imitation_beta=.031673695195944346,
                   imitation_entropy_target=.6716025225602738))
    arm_a, adam_a = hot_policy(source, 'A', torch.device('cpu'))
    sum(p.square().sum() for p in arm_a.parameters()).backward()
    adam_a.step()
    assert all(int(s['step']) == 513 for s in adam_a.state.values())
    assert all(int(s['step']) == 512 for s in source['optimizer']['state'].values())
    arm_b, adam_b = hot_policy(source, 'B', torch.device('cpu'))
    assert all(int(s['step']) == 512 for s in adam_b.state.values())
    for name, value in source['model'].items():
        assert torch.equal(value, arm_b.state_dict()[name])
    assert all(p not in adam_b.state for p in arm_b.combat_value.parameters())
    data = record().repeat(2)
    inputs = packed_inputs(data, torch.arange(3) * len(record()), torch.arange(2), torch.device('cpu'))
    base_logits, base_value = policy(inputs)
    logits, value, combat_value = arm_b(inputs)
    torch.testing.assert_close(logits, base_logits, rtol=0, atol=0)
    torch.testing.assert_close(value, base_value, rtol=0, atol=0)
    torch.testing.assert_close(combat_value, base_value, rtol=0, atol=0)
    (logits.square().sum() + value.sum() + combat_value.sum()).backward()
    adam_b.step()
    assert all(int(adam_b.state[p]['step']) == 1 for p in arm_b.combat_value.parameters())


def test_relative_actor_GAE_raw_critic_targets_selection_and_entropy_limits():
    rng = torch.Generator().manual_seed(1729)
    selected = retain_trials(13, rng)
    assert len(set(selected.tolist())) == 8 and 13 in selected
    advantage, target = combat_gae(torch.full((4,), .4), .8, .5)
    torch.testing.assert_close(advantage, torch.tensor([.3 * .95**3, .3 * .95**2, .3 * .95, .3]))
    assert torch.equal(target, torch.full((4,), .8))
    assert torch.equal(combat_gae(torch.full((4,), .4), .4, .4)[0], torch.zeros(4))
    values = torch.tensor([.2, .4, .3])
    torch.testing.assert_close(combat_gae(values, .8, .5)[0], combat_gae(values + .2, .8, .5)[0])
    assert proportional_beta(.03, .6, .3) == pytest.approx(.12)
    assert proportional_beta(.03, .6, 1.2) == pytest.approx(.0075)
    assert proportional_beta(.03, .6, 0.) == proportional_beta(.03, .6, 1e-300) == .5


def test_dev_seed_rejected_before_new_update_and_restored_minibatch(cpu_group):
    data = fragment()
    data['training_seeds'][0] = 2000000000
    with pytest.raises(AssertionError, match='Dev seed'):
        begin_update(data, torch.Generator(), 4, 'B')
    cursor = dict(done=False, metrics=None, rows=dict(source_partition='train',
                  actions=torch.zeros(1, dtype=torch.int64), training_seeds=torch.tensor([2000000000])))
    with pytest.raises(AssertionError, match='Dev seed'):
        advance_update(None, None, cursor, None, .03)


def test_real_tiny_worker_checkpoint_mid_minibatch_resumes_exactly_and_rejects_tampering(cpu_group, tmp_path, monkeypatch):
    monkeypatch.setattr('train.e4_stage6.copy_learner.MINIBATCH', 2)
    monkeypatch.setattr('train.e4_stage6.copy_learner.MICROBATCH', 1)
    torch.manual_seed(1729)
    def build():
        model = DistributedDataParallel(CombatPolicy(ModelConfig(16, 1, 2, 32)))
        return TrainingWorker(model, torch.optim.Adam(model.parameters(), lr=1e-4), SyntheticCollector(),
            torch.Generator().manual_seed(22), dict(arm='B', rank=0, world=1, bosses_seen=511), .03, .67)
    worker = build()
    worker.step(time.monotonic() + 30)
    worker.step(time.monotonic() + 30)
    assert worker.cursor['optimizer_steps'] == 1 and worker.state['updates'] == 0
    storage = Phase6Storage(tmp_path / 'output', dict(world=1, source='synthetic'))
    reference = storage.save_worker(worker, 'training', 0)
    storage.save_state(dict(checkpoint=reference))
    saved = storage.load_worker(reference)
    tape = saved['collector']['combat_tape']['groups'][0]['trials'][0]['rows'][0]['data']
    assert tape.data_ptr() != worker.collector.tape.groups[0]['trials'][0]['rows'][0]['data'].data_ptr()
    while worker.state['updates'] == 0:
        worker.step(time.monotonic() + 30)
    resumed = build()
    resumed.restore(saved)
    while resumed.state['updates'] == 0:
        resumed.step(time.monotonic() + 30)
    for name, value in worker.model.module.state_dict().items():
        assert torch.equal(value, resumed.model.module.state_dict()[name])
    for (parameter, state), (other, restored) in zip(worker.optimizer.state.items(), resumed.optimizer.state.items()):
        assert torch.equal(parameter, other)
        assert all(torch.equal(value, restored[key]) for key, value in state.items())
    assert torch.equal(worker.ppo_rng.get_state(), resumed.ppo_rng.get_state())
    assert worker.state['rows'][0]['fragment_positions_global'] == 3
    assert worker.state['rows'][0]['actual_valid_mainline_decisions_global'] == 2
    assert worker.state['rows'][0]['retained_copy_decisions_global'] == 1
    assert storage.load_state()['checkpoint'] == reference
    path = storage.root / reference['directory'] / 'rank-0.pt'
    original = path.read_bytes()
    path.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
    with pytest.raises(AssertionError):
        storage.load_worker(reference)


def test_schedule_uses_actual_A_time_and_finishes_both_final_1024_runs():
    schedule = Phase6Schedule(dict(world=7, source='synthetic'))
    action = schedule.next_action(18000, 3600, 210)
    assert action['arm'] == 'A' and action['target_node_seconds'] == 1575
    report = dict(finished=True, target_node_seconds=1575, actual_node_seconds=1575.125, boundary_node_seconds=.125)
    schedule.record_training('A', report, 'A-model')
    assert schedule.next_action(16000, 3600, 210)['target_node_seconds'] == 1575.125
    schedule.record_training('B', dict(report, target_node_seconds=1575.125, actual_node_seconds=1575.145,
                                     boundary_node_seconds=.05), 'B-model')
    schedule.record_examination('A', dict(complete=True, checkpoint='A-model'))
    schedule.record_examination('B', dict(complete=True, checkpoint='B-model'))
    restored = Phase6Schedule(dict(world=7, source='synthetic'))
    restored.restore(schedule.state)
    assert restored.next_action(3600, 3600, 210)['action'] == 'full_run_1024'
    with pytest.raises(AssertionError):
        restored.record_full_run('B', dict(complete=True, episodes=1024, checkpoint='B-model'))
    restored.record_full_run('A', dict(complete=True, episodes=1024, checkpoint='A-model'))
    assert restored.state['phase'] == 'final_B'
    restored.record_full_run('B', dict(complete=True, episodes=1024, checkpoint='B-model'))
    assert restored.state['phase'] == 'complete'


def test_synthetic_report_uses_engine_metric_names_and_writes_gzip_and_18_paths(cpu_group, tmp_path):
    runtime = object.__new__(Phase6Runtime)
    runtime.root, runtime.rank, runtime.world, runtime.job_id = tmp_path, 0, 1, 'synthetic99999'
    runtime.binding = dict(world=1, run_id='fixture', runtime_commit='fixture')
    runtime.store = Phase6Storage(tmp_path, runtime.binding)
    runtime.state = dict(stopped_reason=None)
    search = SimpleNamespace(metrics=dict.fromkeys(SEARCH_METRICS, 0), capacity_metrics=dict.fromkeys(CAPACITY_METRICS, 0),
        loop_metrics=dict.fromkeys(LOOP_METRICS, 0), combat_diagnostics=[], event_trial_max_steps=0, searched_decisions=0)
    search.capacity_metrics.update(combat_trials=1000, combat_played_cards_limit=2)
    episodes = [dict(seed=2000000000+i, floor=1, bosses_seen=511, candidates=[0, 1],
        reach=dict.fromkeys(REACH, False), outcome='death', final_deck_size=12,
        noncombat=dict(counts=dict(card_skipped=1, card_reward_choices=2))) for i in range(1024)]
    reference = dict(directory='generation')
    (tmp_path / reference['directory']).mkdir()
    result = runtime.write_full_run(SimpleNamespace(episodes=episodes, search=search),
        dict(arm='A', round=0, checkpoint='fixture-model'), reference)
    assert result['summary']['combat_played_cards_limit_fraction'] == .002
    assert result['summary']['decision_metrics']['card_skip_rate'] == .5
    assert runtime.state['stopped_reason'] == 'combat_played_cards_limit_above_one_per_thousand'
    directory = tmp_path / reference['directory']
    with gzip.open(directory / result['episodes_file'], 'rt', encoding='utf-8') as stream:
        assert json.load(stream) == episodes
    audit = json.loads(next(directory.glob('*-audit.json')).read_bytes())
    assert len(audit['roots']) == 18
    assert audit['status'] == 'pending_mac_original_replay'
