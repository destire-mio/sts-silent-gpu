import gzip
import json
from pathlib import Path

import pytest
import torch

from train.e4 import PHASE
from train.e4_boss_training import BossTraining, PRACTICE, boss_identity
from train.e4_midstart import MidstartCollector
from train.e4_stage12 import NoSLSearch, collector_snapshot, restore_collector


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 1000., torch.where(kinds == 6, 900., 0.))
        return logits.masked_fill(~inputs["candidate_mask"], -torch.inf), logits.new_full((len(kinds),), .5)


def test_boss_reset_cpu_truncates_and_disabled_is_identical(monkeypatch):
    pytest.importorskip("spire")
    torch.set_num_threads(1)
    device = torch.device("cpu")
    model = EndTurnPolicy().eval()

    def make(fraction=None):
        search = NoSLSearch(1, device, 123, storage_slots=1025 if fraction is not None else 1024)
        collector = MidstartCollector(search, torch.Generator().manual_seed(456), rank=0)
        if fraction is not None:
            collector.boss_training = BossTraining(collector, fraction, 1, 1024, minimum=1)
        return collector

    plain, disabled = make(), make(0.)
    expected, actual = plain.collect(model, 3), disabled.collect(model, 3)
    assert expected.keys() == actual.keys()
    for name in expected:
        assert torch.equal(expected[name], actual[name]), name
    assert torch.equal(plain.seed_rng.get_state(), disabled.seed_rng.get_state())
    assert plain.search.batch.state_bytes(0) == disabled.search.batch.state_bytes(0)

    collector = make(1.)
    search, pool = collector.search, collector.boss_training
    trace = Path(__file__).parent / "traces" / "Transmogrifier-curse-preview-seed2000000764.jsonl.gz"
    prefix = []
    with gzip.open(trace, "rt", encoding="utf-8") as stream:
        seed = json.loads(next(stream))["seed"]
        for line in stream:
            row = json.loads(line)
            state = row["snapshot"]
            action = row["legal"].index(row["action"])
            if state["run"]["floorNum"] == 15 and state["screen"]["type"] == "MAP" and row["action"]["type"] == "choose":
                break
            prefix.append(action)
    collector.reset([0], [seed])
    search.batch.reset_to(0, seed, prefix)
    search.provenance[0] = (seed, prefix.copy())
    search.reset_seen([0])
    collector.paths[0] = prefix.copy()
    before = search.progress_for([0])
    _, terminal = collector.advance([0], [action], before)
    pool.after_step(model, [0], before, terminal, torch.zeros(1), 0)
    root, = pool.entries
    identity = root["boss_identity"]
    assert identity[0] == 1
    saved = search.batch.state_bytes(pool.base)

    collector.seeds[0] = None
    first = collector.collect(model, 1)
    assert collector.starts[0] == PRACTICE and collector.seeds[0] == seed
    assert pool.active[0] == (identity, "act1_boss")
    assert not first["terminal"].item()
    assert search.batch.state_bytes(pool.base) == saved
    snapshot = collector_snapshot(collector)
    resumed = make(1.)
    restore_collector(resumed, snapshot)
    assert resumed.boss_training.active == pool.active
    assert resumed.search.batch.state_bytes(pool.base) == saved
    assert resumed.search.batch.state_bytes(0) == search.batch.state_bytes(0)
    for _ in range(100):
        fight = collector.collect(model, 1)
        if fight["terminal"].item():
            break
    else:
        pytest.fail("Boss-only fight did not truncate")
    assert collector.seeds[0] is None and not collector.completed
    rows = pool.take_metrics()["by_node"]["act1_boss"]
    assert rows == dict(starts=1, completed=1, wins=0, faults=0, win_rate=0.)

    def won(slots, actions, before):
        progress = before[0].copy()
        assert boss_identity(progress) == identity
        progress[PHASE] = 2
        collector.rewards[0].bosses.add(identity)
        search.last_step_progress = {0: progress}
        return [.125], [False]

    monkeypatch.setattr(collector, "advance", won)
    monkeypatch.setattr(search, "scores", lambda model, slots, progress: {0: .5})
    victory = collector.collect(model, 1)
    assert victory["terminal"].item() and victory["rewards"].item() == .125
    assert victory["returns"].item() == .625
    assert collector.seeds[0] is None and not collector.completed
    assert pool.take_metrics()["by_node"]["act1_boss"]["win_rate"] == 1.
