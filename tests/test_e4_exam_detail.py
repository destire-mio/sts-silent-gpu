import time
from types import SimpleNamespace

import torch

from tools.e4_exam_detail import replay, report
from tools.e4_value_truth import collector
from train.e4 import ACT, DEATH, SCREEN, VICTORY
from train.e4_evaluation_ledger import EvaluationLedgerCollector
from train.e4_stage12 import NoSLCollector


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
    reference[seeds[0]] += 1
    rows, verification = replay(model, seeds, reference, device, 2, deadline, verify_count=1)
    assert verification["initial_checked"] == 1
    assert [r["seed"] for r in verification["initial_mismatches"]] == [seeds[0]]
    assert [r["seed"] for r in verification["mismatches"]] == [seeds[0]]
    assert [row["seed"] for row in rows] == seeds
    for row in rows:
        assert row["outcome"] == "death" and row["map"]
        assert row["death"]["deck"] and row["stall_class"] is None
        assert row["reference_floor"] == reference[row["seed"]]
        assert row["fights"][-1]["outcome"] == "death"
        assert row["fights"][-1]["entry_hp"] > row["fights"][-1]["exit_hp"]
        ordinary.search.batch.reset_to(0, row["seed"], row["candidate_indices"])
        assert ordinary.search.progress_for([0])[0][0] == row["final_floor"]
    assert any(row["fights"][-1]["turns"] > 1 for row in rows)
    text = report(rows, verification)
    assert "Deaths 2." in text and "Full replay floor mismatches 1/2." in text
    assert f"| Full | {seeds[0]} |" in text


def test_ledger_keeps_act4_death_separate_from_act3_victory(monkeypatch):
    ledger = EvaluationLedgerCollector.__new__(EvaluationLedgerCollector)
    ledger.records = [dict(counts={"card_removals": 0}, room_entry_hp=20,
                           encounter=[], elite_floors=[], combat_floors=[]) for _ in range(2)]
    ledger.completed = []
    ledger.extra_details = False
    ledger.turns = torch.ones(2, dtype=torch.int64)
    after = {slot: [0] * 9 for slot in range(2)}
    after[0][ACT], after[0][SCREEN] = 4, DEATH
    after[1][ACT], after[1][SCREEN] = 3, VICTORY
    ledger.search = SimpleNamespace(failed={}, last_step_progress=after, arrays={"tags": torch.zeros(2, 2, 1)},
        batch=SimpleNamespace(combat_turns_tensors=lambda turns: turns.fill_(4)),
        frames=lambda slots, progress: {s: dict(actions=[(6, None)], screen=0, deck_size=0) for s in slots})
    monkeypatch.setattr("train.e4_evaluation_ledger.visible_state", lambda search, slot: {"potions": []})

    def finish(self, slots, actions, progress):
        self.completed.extend(dict(floor=51 + s, outcome=outcome,
                                   reach={"act3_second_boss": True, "heart": False})
                              for s, outcome in enumerate(("death", "victory")))
        return [0., 0.], [True, True]

    monkeypatch.setattr(NoSLCollector, "advance", finish)
    ledger.advance([0, 1], [0, 0], after)
    death, win = ledger.completed
    assert death["outcome"] == "death" and death["act2_ledger"]["death"]["turn"] == 4
    assert win["outcome"] == "act3_win" and "death" not in win["act2_ledger"]
