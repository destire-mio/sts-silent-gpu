import io

import torch

from tools.e4_stage12 import TRAINING_MILESTONES, record_training_milestones


def test_standalone_training_milestones_separate_groups_and_resume():
    def episode(reached, outcome="death"):
        return dict(reach={name: i < reached for i, name in enumerate(TRAINING_MILESTONES)},
                    outcome=outcome)

    state = {}
    first = record_training_milestones(
        state, [episode(1), episode(7, "stalled")], [episode(7, "victory"), episode(5)])
    assert tuple(first["fresh"]["round"].values()) == (1, 0, 0, 0, 0, 0, 0)
    assert tuple(first["midstart"]["round"].values()) == (2, 2, 2, 2, 2, 1, 1)

    checkpoint = io.BytesIO()
    torch.save({"state": state}, checkpoint)
    checkpoint.seek(0)
    restored = torch.load(checkpoint, weights_only=True)["state"]
    second = record_training_milestones(restored, [episode(6)], [])
    assert tuple(second["fresh"]["round"].values()) == (1, 1, 1, 1, 1, 1, 0)
    assert tuple(second["fresh"]["cumulative"].values()) == (2, 1, 1, 1, 1, 1, 0)
    assert tuple(second["midstart"]["round"].values()) == (0,) * 7
    assert second["midstart"]["cumulative"] == first["midstart"]["round"]
    assert restored["training_milestones"]["fresh"] == second["fresh"]["cumulative"]
