import gzip
import json
from pathlib import Path

import pytest
import torch

from tools.e4_boss_probe import BossSearch, run_trials
from tools.e4_value_truth import RootCollector
from train.e4 import COMBAT, FLOOR, SCREEN


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 2., torch.where(kinds == 3, 1., 0.))
        return logits.masked_fill(~inputs["candidate_mask"], -torch.inf), logits.new_zeros(len(kinds))


def test_boss_probe_cpu_one_plain_fight():
    pytest.importorskip("spire")
    torch.set_num_threads(1)
    search = BossSearch(1, torch.device("cpu"), 130, 1)
    collector = RootCollector(search, torch.Generator().manual_seed(130))
    trace = Path(__file__).parent / "traces" / "Transmogrifier-curse-preview-seed2000000764.jsonl.gz"
    prefix = []
    with gzip.open(trace, "rt", encoding="utf-8") as stream:
        seed = json.loads(next(stream))["seed"]
        for line in stream:
            row = json.loads(line)
            state = row["snapshot"]
            if state["run"]["floorNum"] == 15 and state["screen"]["type"] == "MAP":
                break
            prefix.append(row["legal"].index(row["action"]))
    collector.reset([0], [seed])
    search.batch.reset_to(0, seed, prefix)
    search.provenance[0] = (seed, prefix.copy())
    search.reset_seen([0])

    model = EndTurnPolicy().eval()
    node = "before_act1_boss"
    root, = collector.capture_roots(model, {node: 1}, [])[node]
    frozen = search.batch.state_bytes(root["slot"])
    trial, = run_trials(collector, model, [(root, 222)])[root["slot"]]
    assert trial["outcome"] in ("win", "death") and trial["turns"] >= 1
    assert trial["end_screen"] != "combat" and trial["end_floor"] == 16
    final = search.progress_for([0])[0]
    assert final[SCREEN] != COMBAT and final[FLOOR] == 16
    assert search.batch.state_bytes(root["slot"]) == frozen
