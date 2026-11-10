import time

import torch

from tools.e4_exam_detail import replay, report
from tools.e4_value_truth import collector


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 100., 0.).masked_fill(~inputs["candidate_mask"], -torch.inf)
        return logits, torch.zeros(len(kinds))


def test_exam_detail_cpu_smoke():
    torch.set_num_threads(1)
    model, device = EndTurnPolicy(), torch.device("cpu")
    seeds = [2_000_000_000, 2_000_000_001]
    deadline = time.monotonic() + 90
    ordinary = collector(2, device, 101, deadline, greedy=True)
    reference = {row["seed"]: row["floor"] for row in ordinary.evaluate(model, seeds)}
    rows, checked = replay(model, seeds, reference, device, 2, deadline, verify_count=1)
    assert checked == 1
    assert [row["seed"] for row in rows] == seeds
    for row in rows:
        assert row["outcome"] == "death" and row["map"]
        assert row["death"]["deck"]
        assert row["fights"][-1]["outcome"] == "death"
        assert row["fights"][-1]["entry_hp"] > row["fights"][-1]["exit_hp"]
        ordinary.search.batch.reset_to(0, row["seed"], row["candidate_indices"])
        assert ordinary.search.progress_for([0])[0][0] == row["final_floor"]
    assert "Deaths 2." in report(rows, checked)
