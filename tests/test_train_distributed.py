from datetime import timedelta
from pathlib import Path
import tempfile
import unittest

import numpy as np
import torch
import torch.distributed as dist
import torch.multiprocessing as mp

from train.distributed import distributed_policy
from train.model import ModelConfig, Policy, distribution, packed_inputs
from train.ppo import PPOConfig, update
from train.rollout import Rollout
from test_train_packing import fixed_observations


def fixed_rollout(model):
    _, data, offsets = fixed_observations()
    rollout = Rollout.allocate(5, 1)
    rollout.count = 5
    rollout.data[:len(data)] = torch.tensor(data.copy())
    rollout.offsets[:] = torch.tensor(offsets.astype(np.int64))
    rollout.actions[:] = torch.tensor([0, 3, 30, 1, 8])
    rollout.returns[:] = torch.tensor([0, 1, 0, 1, 0])
    rollout.valid[:] = True
    rollout.global_ids[:] = torch.arange(5)
    rollout.policy_versions[:] = 3
    with torch.no_grad():
        inputs = packed_inputs(data, offsets, np.arange(5), torch.device("cpu"))
        logits, values = model(inputs)
        policy, _ = distribution(logits, inputs["candidate_mask"])
        rollout.log_probs[:] = policy.log_prob(rollout.actions)
        rollout.values[:] = values
    rollout.advantages[:] = rollout.returns - rollout.values
    return rollout


def train_fixed(model, rollout, optimizer_type=torch.optim.Adam):
    config = PPOConfig(epochs=2, minibatch_size=4)
    optimizer = optimizer_type(model.parameters(), lr=config.learning_rate)
    metrics = update(model, optimizer, rollout, config, torch.device("cpu"), torch.Generator().manual_seed(17),
                     policy_version=3)
    raw = model.module if isinstance(model, torch.nn.parallel.DistributedDataParallel) else model
    return dict(model=raw.state_dict(), gradients={name: p.grad for name, p in raw.named_parameters()},
                optimizer=optimizer.state_dict(), metrics=metrics)


def ddp_worker(rank, world, directory, gate, optimizer_type):
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    with gate:
        torch.manual_seed(1729)
        model = Policy(ModelConfig(width=32, layers=2, heads=4, feedforward=64))
        rollout = fixed_rollout(model)
    dist.init_process_group("gloo", init_method=(Path(directory) / "rendezvous").as_uri(), rank=rank,
                            world_size=world, timeout=timedelta(seconds=30))
    wrapped = distributed_policy(model, torch.device("cpu"))
    rollout.valid[:] = torch.tensor([0, 0, 1, 1, 1]) == rank
    result = train_fixed(wrapped, rollout, optimizer_type)
    torch.save(result, Path(directory) / f"rank-{rank}.pt")
    dist.destroy_process_group()


class DistributedTests(unittest.TestCase):
    def test_ddp_matches_single_process_with_unequal_and_empty_shards(self):
        for optimizer_type in (torch.optim.SGD, torch.optim.Adam):
            self.check_optimizer(optimizer_type)

    def check_optimizer(self, optimizer_type):
        torch.set_num_threads(1)
        torch.manual_seed(1729)
        model = Policy(ModelConfig(width=32, layers=2, heads=4, feedforward=64))
        expected = train_fixed(model, fixed_rollout(model), optimizer_type=optimizer_type)
        for world in (2, 3):
            with self.subTest(world=world, optimizer=optimizer_type.__name__), tempfile.TemporaryDirectory() as directory:
                gate = mp.get_context("spawn").Semaphore(1)
                mp.spawn(ddp_worker, args=(world, directory, gate, optimizer_type), nprocs=world, join=True)
                max_parameter_error = max_gradient_error = 0.
                for rank in range(world):
                    result = torch.load(Path(directory) / f"rank-{rank}.pt", weights_only=True)
                    for name, tensor in expected["model"].items():
                        if name == "score.2.bias" and optimizer_type is torch.optim.Adam:
                            continue
                        actual = result["model"][name]
                        if name.endswith("self_attn.in_proj_bias") and optimizer_type is torch.optim.Adam:
                            width = tensor.numel() // 3
                            print(f"Adam null key bias {name}: {float((actual[width:2*width] - tensor[width:2*width]).abs().max()):.3g}")
                            actual = torch.cat((actual[:width], actual[2*width:]))
                            tensor = torch.cat((tensor[:width], tensor[2*width:]))
                        torch.testing.assert_close(actual, tensor, atol=1e-5, rtol=3e-5, msg=name)
                        max_parameter_error = max(max_parameter_error, float((actual - tensor).abs().max()))
                    for name, tensor in expected["gradients"].items():
                        torch.testing.assert_close(result["gradients"][name], tensor, atol=3e-6, rtol=3e-5)
                        max_gradient_error = max(max_gradient_error, float((result["gradients"][name] - tensor).abs().max()))
                    for index, state in expected["optimizer"]["state"].items():
                        for name, tensor in state.items():
                            torch.testing.assert_close(result["optimizer"]["state"][index][name], tensor, atol=3e-6, rtol=3e-5)
                    for name, value in expected["metrics"].items():
                        if name != "update_seconds":
                            self.assertAlmostEqual(result["metrics"][name], value, delta=3e-6)
                    candidate = Policy(model.config)
                    candidate.load_state_dict(result["model"])
                    _, data, offsets = fixed_observations()
                    inputs = packed_inputs(data, offsets, np.arange(5), torch.device("cpu"))
                    with torch.no_grad():
                        actual_logits, actual_value = candidate(inputs)
                        expected_logits, expected_value = model(inputs)
                    if optimizer_type is torch.optim.SGD:
                        torch.testing.assert_close(actual_logits, expected_logits, atol=3e-6, rtol=3e-5)
                    else:
                        shift = result["model"]["score.2.bias"] - expected["model"]["score.2.bias"]
                        torch.testing.assert_close(actual_logits - shift, expected_logits, atol=3e-6, rtol=3e-5)
                    torch.testing.assert_close(actual_logits.softmax(-1), expected_logits.softmax(-1), atol=3e-6, rtol=3e-5)
                    torch.testing.assert_close(actual_value, expected_value, atol=3e-6, rtol=3e-5)
                print(f"gloo {world} ranks, {optimizer_type.__name__}: identifiable parameters max error "
                      f"{max_parameter_error:.3g}, gradients {max_gradient_error:.3g}")

    def test_stale_sample_rejects_entire_update(self):
        torch.manual_seed(1729)
        model = Policy(ModelConfig(width=32, layers=1, heads=4, feedforward=64))
        rollout = fixed_rollout(model)
        rollout.policy_versions[2] = 2
        before = {name: p.detach().clone() for name, p in model.named_parameters()}
        with self.assertRaisesRegex(AssertionError, "Stale policy samples"):
            train_fixed(model, rollout)
        for name, p in model.named_parameters():
            torch.testing.assert_close(p, before[name], atol=0, rtol=0)


if __name__ == "__main__":
    unittest.main()
