from copy import deepcopy
import gzip
import json
from pathlib import Path
from types import SimpleNamespace

import pytest
import torch

from train.e4 import RoomSearch, COMBAT_PHASE, PHASE, SCREEN, DEATH
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.examination import load_roots
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_teacher import CombatTeacher
from train.model import FIELDS, ModelConfig
from tools.e4_teacher import allocation_deadline


class TinySearch:
    def __init__(self, turn_after, finish_after):
        self.device = torch.device('cpu')
        self.size, self.lanes, self.inference_chunk = 33, 1, 128
        self.available = list(range(1, 33))
        self.rng = torch.Generator().manual_seed(1729)
        self.arrays = dict.fromkeys(FIELDS)
        self.turns, self.steps = [1] * 33, [0] * 33
        self.first = [0] * 33
        self.turn_after, self.finish_after = turn_after, finish_after
        self.determinized = []
        self.batch = SimpleNamespace(combat_turns_tensors=lambda out:out.copy_(torch.tensor(self.turns)),
                                     determinize_slots=self.determinize)

    def clock(self):
        pass

    def take(self, count):
        result = self.available[-count:]
        del self.available[-count:]
        return result

    def clone(self, sources, targets):
        for source, target in zip(sources, targets):
            self.steps[target], self.turns[target] = self.steps[source], self.turns[source]

    def determinize(self, slots, seeds):
        self.determinized.append((slots.copy(), seeds.copy()))

    def check(self, slots):
        pass

    def step(self, slots, actions):
        for slot, action in zip(slots, actions):
            if self.steps[slot] == 0:
                self.first[slot] = action
            self.steps[slot] += 1
            self.turns[slot] = 1 + self.steps[slot] // self.turn_after

    def progress(self):
        result = [[0] * 9 for _ in range(self.size)]
        for slot, row in enumerate(result):
            row[PHASE] = COMBAT_PHASE
            row[SCREEN] = DEATH if self.steps[slot] >= self.finish_after else 0
        return result

    def predict(self, model, slots):
        torch.rand(1, generator=self.rng)
        return [0] * len(slots), [], []

    def scores(self, model, slots, progress):
        return dict.fromkeys(slots, 0.)


@pytest.mark.parametrize('horizon,turn_after,finish_after,steps,boundaries',[
    ('turn', 3, 999, 3, 32), ('turn', 9, 2, 2, 0), ('combat', 2, 999, 300, 32)])
def test_common_seed_candidates_turn_boundary_terminal_death_and_300_step_cap(
        monkeypatch, horizon, turn_after, finish_after, steps, boundaries):
    search = TinySearch(turn_after, finish_after)
    def inputs(arrays, device, slots):
        return dict(slots=slots, candidate_mask=torch.ones(len(slots), 2, dtype=torch.bool))
    monkeypatch.setattr('train.e4_teacher.network_inputs', inputs)
    class Policy:
        def __call__(self, inputs):
            slots = inputs['slots'].tolist()
            return (torch.tensor([[3., 2.]] * len(slots)), torch.zeros(len(slots)),
                    torch.tensor([float(search.first[s]) for s in slots]))
        def run_policy(self, inputs):
            return self(inputs)[:2]
    rng_before = search.rng.get_state().clone()
    source = deepcopy((search.steps[0], search.turns[0]))
    teacher = CombatTeacher(search, torch.Generator().manual_seed(94), horizon)
    actions, rows = teacher.select(Policy(), [0])
    assert actions == ([0] if boundaries == 0 else [1])
    assert rows[0]['preview_steps'] == steps * 32 and rows[0]['value_boundaries'] == boundaries
    assert rows[0]['candidates'] == [0, 1]
    assert search.determinized[0][1] == list(range(16)) * 2
    assert all(s > 0 for s in search.determinized[0][0])
    assert source == (search.steps[0], search.turns[0])
    assert torch.equal(rng_before, search.rng.get_state())
    assert len(set(search.available)) == 32


@pytest.mark.parametrize('horizon', ['turn', 'combat'])
def test_discard_probe_is_record_only_and_restores_both_generators(monkeypatch, horizon):
    search = TinySearch(2, 4)
    def inputs(arrays, device, slots):
        return dict(slots=slots, candidate_mask=torch.ones(len(slots), 2, dtype=torch.bool),
            candidate_kinds=torch.tensor([[1, 5]] * len(slots)),
            candidate_refs=torch.tensor([[[0, 1]]] * len(slots)),
            ids=torch.tensor([[[8]]] * len(slots)))
    monkeypatch.setattr('train.e4_teacher.network_inputs', inputs)
    class Policy:
        def __call__(self, inputs):
            n = len(inputs['slots'])
            return torch.tensor([[3., 2.]] * n), torch.zeros(n), torch.ones(n)
        def run_policy(self, inputs):
            return self(inputs)[:2]
    teacher = CombatTeacher(search, torch.Generator().manual_seed(95), horizon)
    records = [dict(candidates=[0], candidate_mean_scores=[3.], selected=0)]
    actor, rollout = search.rng.get_state().clone(), teacher.rollout_rng.get_state().clone()
    teacher.discard_probes(Policy(), [0], records)
    probe = records[0]['potion_discard_probes'][0]
    assert probe['potion_id'] == 7 and probe['discard_action'] == 1
    assert probe['preview_trials'] == 16 and probe['selected_minus_discard'] == (2. if horizon == 'turn' else 3.)
    assert search.steps[0] == 0 and search.turns[0] == 1
    assert torch.equal(actor, search.rng.get_state()) and torch.equal(rollout, teacher.rollout_rng.get_state())
    assert search.determinized[0][1] == list(range(16)) and len(search.available) == 32


def test_potion_summary_counts_wins_and_each_bottle_probe():
    from train.e4_teacher_pilot import summarize
    rows = [dict(group='act1_elite', actions=[0], death=False, won=True, score=2.,
        decision_seconds=1., potion_probe_seconds=.5, potion_uses=2, potions_remaining=1,
        decision_timings=[dict(potion_discard_probes=[dict(potion_id=7, selected_minus_discard=.2),
                                                   dict(potion_id=7, selected_minus_discard=.4)])]),
        dict(group='act1_elite', actions=[0, 1], death=True, won=False, score=0.,
        decision_seconds=2., potion_probe_seconds=0., potion_uses=0, potions_remaining=2,
        decision_timings=[dict(potion_discard_probes=[])])]
    summary = summarize(rows)['act1_elite']
    assert summary['mean_potion_uses'] == 1 and summary['won_combats'] == 1
    assert summary['mean_potions_remaining_when_won'] == 1 and summary['potion_probe_count'] == 2
    assert summary['mean_selected_minus_discard'] == pytest.approx(.3)
    assert summary['potion_probe_top10'][0]['potion_id'] == 7


def test_native_teacher_selection_preserves_real_Game_History_all_RNG_and_policy():
    torch.set_num_threads(1)
    torch.manual_seed(94)
    device = torch.device('cpu')
    search = RoomSearch(1, 32, device, torch.Generator().manual_seed(1729), scratch_slots=128, bosses_seen=511)
    path = Path(__file__).resolve().parents[1] / 'inventory/e4-c88-stage6-combat-exam.json.gz'
    with gzip.open(path, 'rt') as stream:
        scenes = json.load(stream)['groups']['act1_boss_guardian']
    for scene in scenes:
        load_roots(search, [scene])
        if bool((search.arrays['candidate_kinds'][0] == 5).any()):
            break
    assert bool((search.arrays['candidate_kinds'][0] == 5).any()), 'Need a real held-potion examination root'
    policy = CombatPolicy(ModelConfig(16, 1, 2, 32)).eval().requires_grad_(False)
    before = search.batch.state_bytes(0)
    rng = search.rng.get_state().clone()
    provenance = deepcopy(search.provenance[0])
    model = policy_hash(policy)
    original = search.batch
    calls = []
    class RecordedBatch:
        def __getattr__(self, name):
            return getattr(original, name)
        def determinize_slots(self, slots, seeds):
            calls.append((slots.copy(), seeds.copy()))
            assert all(s >= search.lanes for s in slots)
            original.determinize_slots(slots, seeds)
    search.batch = RecordedBatch()
    teacher = CombatTeacher(search, torch.Generator().manual_seed(94), 'turn')
    actions, records = teacher.select(policy, [0])
    teacher.discard_probes(policy, [0], records)
    assert records[0]['potion_discard_probes']
    assert search.batch.state_bytes(0) == before
    assert torch.equal(search.rng.get_state(), rng) and search.provenance[0] == provenance
    assert policy_hash(policy) == model and all(p.grad is None for p in policy.parameters())
    assert len(calls) == 1 + len(records[0]['potion_discard_probes'])
    assert calls[0][1] == list(range(16)) * len(records[0]['candidates'])
    assert all(seeds == list(range(16)) for _, seeds in calls[1:])
    assert actions[0] in records[0]['candidates'] and records[0]['preview_steps'] >= len(calls[0][0])
    assert len(search.available) == 128


def test_teacher_allocation_requires_fresh_real_receipt_and_new_3600_cap(tmp_path):
    import hashlib
    import time
    def record(name, data):
        path = tmp_path / name
        path.write_bytes(data)
        return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest())
    receipt = record('receipt', b'Submitted batch job 123\n')
    proof = record('proof', b'original proof')
    now = time.time()
    context = dict(format=1, phase='claude94_stage7', runtime_commit='source',
        receipt=dict(job_id=123,world=7,wall_seconds=4080,at=now-4,stdout=receipt),
        reservation=dict(job_id=123,at=now-3,node_seconds=3570,parent_ledger=proof),
        release=dict(job_id=123,at=now-2,budget_at=now-5,preflight_at=now-5,preflight=proof),
        allocation=dict(job_id=123,world=7,cpus=8,start_at=now-1,end_at=now+4079,
                        wall_seconds=4080,scontrol=proof),
        budgets=dict(claude94_stage7=dict(cap_node_seconds=3600,charged_node_seconds=0,reserved_node_seconds=3570)))
    assert allocation_deadline(context, '123', 7, 'source') > time.monotonic()
    for field in ('budget_at', 'preflight_at'):
        stale = deepcopy(context)
        stale['release'][field] = now-400
        with pytest.raises(AssertionError):
            allocation_deadline(stale, '123', 7, 'source')
    context['budgets']['claude94_stage7']['cap_node_seconds'] = 18000
    with pytest.raises(AssertionError):
        allocation_deadline(context, '123', 7, 'source')
