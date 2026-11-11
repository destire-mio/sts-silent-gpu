from unittest.mock import patch

import torch

from train.e4_stage12 import apply_strategic_share
from train.e4_late_pools import stage_root_is_heldout


def test_strategic_share_preserves_forced_and_invalid_rows():
    fragment = dict(advantages=torch.ones(8), action_valid=torch.tensor([True] * 7 + [False]),
                    policy_mask=torch.tensor([[True, True]] * 6 + [[True, False], [True, True]]),
                    decision_types=torch.tensor([1, 2, 0, 0, 5, 0, 1, 1]))
    cursor = dict(fragment=fragment)
    with patch("train.e4_stage12.dist.all_reduce"):
        apply_strategic_share(cursor, .35)
    weighted = fragment["advantages"]
    assert torch.isclose(weighted[:2].sum() / weighted[:6].sum(), torch.tensor(.35))
    assert torch.equal(weighted[6:], torch.ones(2))
    assert cursor["strategic_share"]["share_before"] == 2 / 6


def test_strategic_share_uses_global_counts_and_handles_absent_groups():
    fragment = dict(advantages=torch.tensor([2., -3.]), action_valid=torch.ones(2, dtype=torch.bool),
                    policy_mask=torch.ones((2, 2), dtype=torch.bool), decision_types=torch.tensor([1, 0]))
    cursor = dict(fragment=fragment)
    with patch("train.e4_stage12.dist.all_reduce", side_effect=lambda counts: counts.copy_(torch.tensor([2., 8.]))):
        apply_strategic_share(cursor, .35)
    assert torch.allclose(fragment["advantages"], torch.tensor([2.8, -2.6]))
    fragment["decision_types"].zero_()
    before = fragment["advantages"].clone()
    with patch("train.e4_stage12.dist.all_reduce"):
        apply_strategic_share(cursor, .35)
    assert torch.equal(fragment["advantages"], before)


def test_late_pool_holdout_uses_the_replayed_root_identity():
    rows = [dict(root=dict(seed=1_000_000_000 + i, candidates=[1, 2, i])) for i in range(100)]
    selected = [stage_root_is_heldout(row) for row in rows]
    assert 5 < sum(selected) < 40
    assert selected == [stage_root_is_heldout(dict(stage="heart_start", **row)) for row in rows]
    assert not any(stage_root_is_heldout(row, 0.) for row in rows)
