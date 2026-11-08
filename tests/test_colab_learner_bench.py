import torch

from tools.colab_learner_bench import parse_args, run


def test_colab_learner_cpu_smoke():
    torch.set_num_threads(1)
    args = parse_args(["--width", "384", "--lanes", "2", "--rollout-length", "4", "--microbatch", "2"])
    # Use Dynamo's eager backend to exercise dynamic capture without requiring an Inductor C++ toolchain.
    real_compile = torch.compile
    calls = []

    def compile_eager(fn, *, dynamic):
        calls.append(dynamic)
        return real_compile(fn, dynamic=dynamic, backend="eager")

    from unittest.mock import patch
    with patch.object(torch, "compile", compile_eager):
        base, compiled = run(args, torch.device("cpu"), ((False, False), (True, False)))
    assert calls and all(calls)
    assert base["valid_examples"] == compiled["valid_examples"] > 0
    assert base["matches_baseline"] and compiled["matches_baseline"]
