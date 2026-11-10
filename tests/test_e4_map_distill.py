import torch

from train.e4_map_distill import MapDistillPolicy, train_head_step
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


def test_map_distill_zero_weight_identity_and_gradient_isolation():
    config = ModelConfig(24, 1, 4, 48)
    device = torch.device("cpu")
    torch.manual_seed(4)
    original = CLSPolicy(config).eval()
    zero = MapDistillPolicy(config, 0., device).eval()
    active = MapDistillPolicy(config, 1., device).eval()
    for policy in (zero, active):
        policy.load_state_dict(original.state_dict(), strict=True)
    observation = dict(
        tags=torch.zeros(1, 4, 3, dtype=torch.long),
        ids=torch.tensor([[[10, 12, 13], [0, 0, 0], [0, 0, 0], [0, 0, 0]]]),
        values=torch.zeros(1, 6, 3),
        entity_mask=torch.ones(1, 3, dtype=torch.bool),
        candidate_kinds=torch.tensor([[3, 3]]),
        candidate_refs=torch.tensor([[[0, 0], [0, 0], [2, 3]]]),
        candidate_mask=torch.ones(1, 2, dtype=torch.bool),
    )
    expected, expected_value = original(observation)
    actual, actual_value = zero(observation)
    assert torch.equal(actual.view(torch.int32), expected.view(torch.int32))
    assert torch.equal(actual_value.view(torch.int32), expected_value.view(torch.int32))
    optimizer = torch.optim.Adam(active.route_head.parameters(), lr=3e-4)
    base_before = {name: p.detach().clone() for name, p in active.named_parameters()}
    step = train_head_step(active, optimizer, observation, torch.tensor([[3., -3.]]))
    assert step["loss"] > 0 and step["head_mean_abs"] > 0
    assert bool(active.route_head.weight.ne(0.).any())
    assert all(p.grad is None and torch.equal(p, base_before[name])
               for name, p in active.named_parameters())
    optimizer.zero_grad(set_to_none=True)
    active(observation)[0].sum().backward()
    assert all(p.grad is None for p in active.route_head.parameters())
    assert any(p.grad is not None for p in active.parameters())
