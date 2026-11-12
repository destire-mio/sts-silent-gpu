from types import SimpleNamespace
from unittest.mock import patch

import torch

from tools import e4_search_labels as labels
from tools.e4_map_labels import read_json
from train.e4 import ACT, AllocationEnded, FLOOR, ROOM
from train.e4_combat_distill import load_combat_targets


def preview_fixture():
    progress = [0] * 9
    progress[ACT] = 1
    progress[FLOOR] = 7
    progress[ROOM] = 2
    search = SimpleNamespace(
        failed={1: "preview fault"},
        progress_for=lambda slots: {slot: progress for slot in slots},
        provenance_record=lambda slot: (labels.FIRST_SEED + slot, [2, 1]),
        reward_ledgers=[SimpleNamespace(reach={"act3_first_boss": False})],
    )
    report = dict(preview_steps=32, candidates=[5, 2], selected=2, seeds=list(range(8)),
                  crn_scores=[[0.2] * 8, [0.4] * 8], reference_actions=[2, 3, 5],
                  reference_logits=[0.2, -0.9, 1.2])
    return search, report


def test_search_label_shards_load_as_combat_and_card(tmp_path):
    search, report = preview_fixture()
    writer = labels.LabelWriter(tmp_path, {"mode": "search_teacher"}, cards=True)
    with patch.object(labels, "pack_observations", return_value=["AQID"]):
        for kind in ("combat", "card"):
            writer.observe(search, kind, [0, 1], [report, report])
    writer.flush()
    for kind in ("combat", "card"):
        targets, _, summary = load_combat_targets(tmp_path / kind)
        assert summary["roots"] == 1
        assert summary["source"] == kind
        assert targets[0][0] == b"\x01\x02\x03"
        row = read_json(tmp_path / kind / "labels-0000000.json.gz")["rows"][0]
        assert row["argmax"] == 5 and row["selected"] == 2
        assert row["draws"] == list(range(8))
        assert [option["k"] for option in row["estimates"]] == [8, 8]


def test_search_label_deadline_flushes_unfinished_game(tmp_path):
    search, report = preview_fixture()
    writer = labels.LabelWriter(tmp_path, {"mode": "search_teacher"}, cards=False)

    class InterruptedGame:
        round_games = 1
        completed = []
        search = SimpleNamespace(lookahead=SimpleNamespace(simulated_steps=32),
                                 card_lookahead=None, device_peak_used=None)

        def evaluate(self, model, seeds):
            with patch.object(labels, "pack_observations", return_value=["AQID"]):
                writer.observe(search, "combat", [0], [report])
            assert not list((tmp_path / "combat").glob("labels-*.json.gz"))
            raise AllocationEnded()

    collector = InterruptedGame()
    assert labels.collect_teacher_labels(collector, None, [labels.FIRST_SEED], writer) == "time_budget"
    _, _, summary = load_combat_targets(tmp_path / "combat")
    assert summary["roots"] == 1
    report = labels.report_metrics(writer, collector, 1., torch.device("cpu"), 1)
    assert report["episodes_completed"] == 0 and report["labels_from_unfinished_episodes"] == 1
    assert report["preview_steps"] == 32
