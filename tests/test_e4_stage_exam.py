import math

from tools.e4_stage_exam import paired_stage_summary, select_e2_sources, stage_summary
from train.e4_late_pools import stage_root_is_heldout


def test_stage_sources_are_excluded_from_late_training():
    rows = [dict(partition="train", stage="heart_start", root=dict(seed=1_000_000_000 + i, candidates=[i]))
            for i in range(100)]
    selected, partition = select_e2_sources(rows, 256)
    assert selected and partition == "train_root_hash_holdout_0.2"
    assert all(stage_root_is_heldout(row) for row in selected)
    dev = dict(rows[0], partition="dev")
    assert select_e2_sources(rows + [dev], 256)[0] == [dev]


def test_stage_exam_SE_uses_roots_and_four_matching_futures():
    reference = [dict(root=root, draw=draw, stage="heart", complete=True, won=False,
                      hp_lost=10, potions_used=1) for root in ("a", "b") for draw in range(4)]
    rows = [dict(row, won=row["root"] == "a", hp_lost=5) for row in reference]
    stats = stage_summary(rows)["heart"]
    assert stats["win_rate"] == .5 and math.isclose(stats["win_rate_SE"], .5)
    paired = paired_stage_summary(rows, reference)["heart"]
    assert paired["paired_roots"] == 2 and paired["paired_draws"] == 8
    assert paired["win_rate_delta"] == .5 and math.isclose(paired["paired_SE"], .5)
    reference[0]["complete"] = False
    assert paired_stage_summary(rows, reference)["heart"]["paired_roots"] == 1
