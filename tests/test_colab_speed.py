import torch

from tools.colab_speed import parse_args, run


def test_cpu_round_with_random_weights():
    args = parse_args(["--width", "384", "--lanes", "2", "--microbatch", "64", "--rounds", "1"])
    assert args.checkpoint is None
    row, = run(args, torch.device("cpu"))
    assert row["round"] == 1
    assert row["decisions"] == 2 * 64
    assert row["faults"] == 0
    assert row["rollout_seconds"] > 0 and row["learner_seconds"] > 0
    assert 0 < row["decisions_per_second"] < row["rollout_decisions_per_second"]
    assert row["peak_allocated_bytes"] == row["peak_reserved_bytes"] == 0
