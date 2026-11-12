import math
import statistics
from unittest import TestCase

import torch

from tools.e4_map_distill_offline import split_by_seed
from tools.e4_map_labels import label_options, summary
from train.e4_map_distill import MapDistillPolicy, target_rows, train_head_step
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


class MapDistillTests(TestCase):
    def test_zero_weight_identity_and_gradient_isolation(self):
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

    def test_crn_paired_variance_increases_target_shift(self):
        reference = [.25, .4, .55, .7] * 3
        jitter = [-.02, 0., .02, 0.] * 3
        samples = {0: reference, 1: [x + .20 + noise for x, noise in zip(reference[:4], jitter)],
                   2: [x - .20 + noise for x, noise in zip(reference, jitter)]}
        estimates = label_options(samples, 0, [2])
        row = dict(act=1, argmax=0, options=[0, 1, 2], p_ref_logits=[0., 0., 0.],
                   packed_observation="", estimates=estimates, complete=True,
                   scores={str(action): values for action, values in samples.items()})
        targets, variances = target_rows([row])
        options = estimates[1:]
        degrees = sum(o["k"] - 1 for o in options)
        old_var = sum((o["k"] - 1) * o["rollout_sd"]**2 for o in options) / degrees
        old_signal = statistics.variance(o["delta"] for o in options) - statistics.mean(
            old_var / o["k"] for o in options)
        old_se_sq = old_var / options[0]["k"]
        old_z = options[0]["delta"] * old_signal / (old_signal + old_se_sq) / math.sqrt(old_se_sq)
        expected = sum((o["k"] - 1) * o["paired_se"]**2 * o["k"] for o in options) / degrees
        self.assertAlmostEqual(variances[1][0], expected)
        self.assertGreater(targets[0][2][1], old_z)
        report = summary([dict(rows=[row], seconds=1., preview_steps=10)], "labels", {})["crn_by_act"]["act1"]
        self.assertGreater(report["paired_correlation"], .99)
        self.assertAlmostEqual(report["paired_to_rollout_var"], expected / old_var)


    def test_offline_holdout_groups_seeds_after_excluding_gold(self):
        rows = [dict(seed=seed, act=act) for seed in range(11) for act in (1, 2)]
        train, validation = split_by_seed(rows, {0})
        train_seeds = {row["seed"] for row in train}
        validation_seeds = {row["seed"] for row in validation}
        self.assertEqual(len(train_seeds), 9)
        self.assertEqual(len(validation_seeds), 1)
        self.assertEqual(len(validation), 2)
        self.assertFalse(train_seeds & validation_seeds)
        self.assertEqual(train_seeds | validation_seeds, set(range(1, 11)))
