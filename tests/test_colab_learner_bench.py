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



def test_compiled_learner_supports_retained_graph_for_value_calibration(monkeypatch):
    from torch._functorch import config
    from train.colab_learner import configure_learner

    monkeypatch.setattr(config, "donated_buffer", True)
    real_compile = torch.compile

    def compile_aot(fn, *, dynamic):
        return real_compile(fn, dynamic=dynamic, backend="aot_eager")

    monkeypatch.setattr(torch, "compile", compile_aot)
    policy = torch.nn.Module()
    policy.encoder = torch.nn.ModuleList([
        torch.nn.Sequential(torch.nn.Linear(4, 4), torch.nn.Tanh(), torch.nn.Linear(4, 2))
    ])
    inputs = torch.arange(12, dtype=torch.float32).reshape(3, 4)
    layer = policy.encoder[0]
    params = tuple(layer.parameters())

    def gradients():
        actor, value = layer(inputs).unbind(-1)
        return (
            torch.autograd.grad(actor.sum(), params, retain_graph=True),
            torch.autograd.grad(value.square().sum(), params),
        )

    eager = gradients()
    configure_learner(policy, compile_encoder=True)
    compiled = gradients()
    assert config.donated_buffer is False
    for expected, actual in zip(eager, compiled):
        for eager_grad, compiled_grad in zip(expected, actual):
            torch.testing.assert_close(compiled_grad, eager_grad)
