from pathlib import Path
import gzip
import json

import pytest
import torch

from train import e4
from train.e4_stage6.combat import CombatPolicy, retain_trials
from train.model import ModelConfig


def test_uniform_retention_does_not_force_the_adopted_trial():
    counts = torch.zeros(32, dtype=torch.int64)
    rng = torch.Generator().manual_seed(101)
    for _ in range(1024):
        selected = retain_trials(rng)
        assert len(set(selected.tolist())) == 8
        counts[selected] += 1
    assert counts.min() > 180 and counts.max() < 340


def test_waiting_combat_preserves_observation_action_and_GAE_alignment(monkeypatch):
    class Search:
        device = torch.device('cpu')
        lanes = 2
        arrays = {}
        batch = None
        steps = [0, 0]
        phases = [e4.COMBAT_PHASE, 0]
        groups = []

        def clock(self):
            pass

        def progress_for(self, slots):
            return {s: [0, 1, self.phases[s], 0, 0 if self.phases[s] else 10, 0, 0, 0, 50] for s in slots}

        def predict(self, model, slots):
            self.actual_policy_logs = [[0.] * 128 for _ in slots]
            return [0] * len(slots), [float(10 * s + self.steps[s]) for s in slots], [0.] * len(slots)

        def policy_masks(self, slots):
            return torch.ones(len(slots), 128, dtype=torch.bool)

        def plan(self, model, slots, kind):
            assert kind == 'combat'
            self.groups.append(slots.copy())
            return {s: [1] for s in slots}

        def scores(self, model, slots, progress):
            return {s: float(10 * s + self.steps[s]) for s in slots}

    search = Search()
    collector = e4.SegmentCollector(search, torch.Generator())
    collector.seeds = [1000000000, 1000000001]

    def store(batch, arrays, slots, data, offsets):
        start = int(offsets[0])
        for i, slot in enumerate(slots.tolist()):
            data[start + i] = 100 * slot + search.steps[slot]
            offsets[i + 1] = start + i + 1

    def advance(slots, actions, progress):
        for slot in slots:
            search.steps[slot] += 1
            search.phases[slot] = e4.COMBAT_PHASE if slot == 1 and search.steps[slot] == 1 else 0
        return [1.] * len(slots), [False] * len(slots)

    monkeypatch.setattr(e4, 'store_inputs', store)
    collector.advance = advance
    result = collector.collect(None, length=2)
    assert search.groups == [[0, 1]]
    assert result['data'][result['offsets'][:-1]].tolist() == [100, 0, 101, 1]
    assert result['actions'].tolist() == [0, 1, 1, 0]
    assert result['values'].tolist() == [10., 0., 11., 1.]
    torch.testing.assert_close(result['advantages'], torch.tensor([3.98, 3.98, 2., 2.]))
    assert search.steps == [2, 2] and collector.partial is None


def test_uniform_retained_replay_does_not_overwrite_the_adopted_native_copy(monkeypatch):
    pytest.importorskip('spire')
    from train.e4_stage6.copy_search import CopySearch
    from train.e4_stage6.examination import load_roots
    seed = next(s for s in range(100) if 0 not in retain_trials(torch.Generator().manual_seed(s)))
    torch.manual_seed(99)
    torch.set_num_threads(1)
    search = CopySearch(1, torch.device('cpu'), torch.Generator().manual_seed(99),
                        torch.Generator().manual_seed(seed))
    with gzip.open(Path(__file__).resolve().parents[1] / 'inventory/e4-c88-stage6-combat-exam.json.gz', 'rt') as stream:
        scene = json.load(stream)['groups']['act1_boss_guardian'][:1]
    roots = load_roots(search, scene)
    policy = CombatPolicy(ModelConfig(16, 1, 2, 32)).eval().requires_grad_(False)
    policy.value[2].weight.zero_()
    policy.value[2].bias.zero_()
    replay = search.tape.replay_retained
    def check(search, context, slots, selected, parent, winner):
        assert slots.index(winner) == 0 and 0 not in selected
        original = search.batch.state_bytes(winner)
        replay(search, context, slots, selected, parent, winner)
        assert search.batch.state_bytes(winner) == original
    monkeypatch.setattr(search.tape, 'replay_retained', check)
    paths = search.combat(policy, roots)
    assert paths[0] and len(search.tape.groups[0]['trials']) == 8
    assert not any(r['adopted'] for r in search.tape.groups[0]['trials'])
