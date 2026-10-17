from dataclasses import asdict
from pathlib import Path
import tempfile
import unittest

import numpy as np
import torch

from train.model import ModelConfig, Policy, packed_inputs, distribution
from train.pipeline import Pipeline, PipelineConfig
from train.ppo import PPOConfig
from train.rollout import Collector


class PipelineTests(unittest.TestCase):
    def test_two_actors_two_learners_publish_versions_and_keep_trajectories(self):
        torch.set_num_threads(1)
        torch.manual_seed(1729)
        model_config = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
        ppo_config = PPOConfig(epochs=2, minibatch_size=7)
        config = PipelineConfig(actors=2, learners=2, batch_size=2, decision_cap=4, storage_mib=8, torch_threads=1)
        model = Policy(model_config)
        roots = [dict(seed=1000000000 + i, candidates=[]) for i in range(5)]
        expected = Collector(2, 4, 4, torch.device("cpu"), 1).collect(model, roots, 1729, range(5), policy_version=0)
        manifest = dict(model=asdict(model_config), ppo=asdict(ppo_config), engine_commit="test", observation_version=2)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            with Pipeline(model_config, ppo_config, config, 1729, output, manifest) as pipeline:
                pipeline.initialize()
                first = pipeline.collect(roots, 1729, range(5))
                self.assertEqual(first.count, 20)
                self.assertTrue(first.valid.all())
                self.assertEqual([e["candidates"] for e in first.episodes], [e["candidates"] for e in expected.episodes])
                self.assertEqual([e["outcome"] for e in first.episodes], ["truncated"] * 5)
                np.testing.assert_array_equal(np.sort(np.concatenate([e["records"] for e in first.episodes])), np.arange(20))
                for actual, reference in zip(first.episodes, expected.episodes):
                    np.testing.assert_allclose(first.returns[actual["records"]], expected.returns[reference["records"]], atol=2e-6)
                inputs = packed_inputs(first.data, first.offsets, np.arange(first.count), torch.device("cpu"))
                model.eval()
                with torch.no_grad():
                    logits, values = model(inputs)
                    policy, _ = distribution(logits, inputs["candidate_mask"])
                np.testing.assert_allclose(policy.log_prob(torch.tensor(first.actions)).numpy(), first.log_probs, atol=2e-6)
                np.testing.assert_allclose(values.numpy(), first.values, atol=2e-6)
                metrics = pipeline.update(first)
                self.assertEqual((metrics["policy_version"], metrics["published_version"]), (0, 1))
                checkpoint = torch.load(output / "checkpoint.pt", weights_only=False)
                self.assertEqual(checkpoint["policy_version"], 1)
                for name, value in pipeline.state.items():
                    torch.testing.assert_close(value, checkpoint["model"][name], atol=0, rtol=0)
                second = pipeline.collect(roots, 1729, range(5, 10), decision_budget=5)
                self.assertEqual(second.count, 5)
                self.assertFalse(second.valid.any())
                np.testing.assert_array_equal(second.policy_versions, np.ones(5))
                metrics = pipeline.update(second)
                self.assertEqual(metrics["samples"], 0)
                self.assertEqual(pipeline.version, 2)
                model.load_state_dict(pipeline.state)
                third = pipeline.collect(roots, 1729, range(10, 15))
                inputs = packed_inputs(third.data, third.offsets, np.arange(third.count), torch.device("cpu"))
                with torch.no_grad():
                    logits, values = model(inputs)
                    policy, _ = distribution(logits, inputs["candidate_mask"])
                np.testing.assert_allclose(policy.log_prob(torch.tensor(third.actions)).numpy(), third.log_probs, atol=2e-6)
                np.testing.assert_allclose(values.numpy(), third.values, atol=2e-6)
                self.assertTrue(all(e["policy_version"] == 2 for e in third.episodes))
                pipeline.update(third)
            self.assertTrue(all(not p.is_alive() and p.exitcode == 0 for p in pipeline.processes))


if __name__ == "__main__":
    unittest.main()
