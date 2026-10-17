import ast
from pathlib import Path
import unittest

import numpy as np
import spire
import torch

from train.evaluate import compare, evaluate, outcome_summary
from train.model import ModelConfig, Policy, distribution, network_inputs, views
from train.ppo import PPOConfig, objective, update
from train.rollout import Collector, episode_targets


torch.set_num_threads(1)
torch.set_num_interop_threads(1)
ROOT = Path(__file__).resolve().parents[1]


class TrainerTests(unittest.TestCase):
    def setUp(self):
        torch.manual_seed(1729)
        self.device = torch.device("cpu")
        self.model = Policy(ModelConfig(width=32, layers=1, heads=4, feedforward=64))

    def test_rewarded_candidate_probability_increases(self):
        logits = torch.nn.Parameter(torch.zeros(2))
        value = torch.nn.Parameter(torch.tensor(0.0))
        optimizer = torch.optim.Adam([logits, value], lr=0.03)
        actions = torch.tensor([0, 1] * 16)
        returns = (actions == 0).float()
        old = torch.full((32,), -np.log(2), dtype=torch.float32)
        config = PPOConfig(entropy_coefficient=0)
        for _ in range(20):
            loss, _ = objective(logits.expand(32, -1), value.sigmoid().expand(32), torch.ones(32, 2, dtype=torch.bool),
                                actions, old, returns, returns - 0.5, config)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
        probability = float(logits.detach().softmax(-1)[0])
        self.assertGreater(probability, 0.6)
        print(f"synthetic rewarded candidate: 0.500000 -> {probability:.6f}")

    def test_returns_truncation_and_lambda(self):
        values = np.array([0.2, 0.4, 0.8])
        returns, advantages = episode_targets([0, 0, 1], values, 0, 1)
        np.testing.assert_allclose(returns, [1, 1, 1])
        np.testing.assert_allclose(advantages, 1 - values)
        returns, advantages = episode_targets([0, 0, 0], values, 0.7, 1)
        np.testing.assert_allclose(returns, [0.7, 0.7, 0.7])
        np.testing.assert_allclose(advantages, 0.7 - values)
        returns, advantages = episode_targets([0, 0, 1], values, 0, 0)
        np.testing.assert_allclose(advantages, [0.2, 0.4, 0.2])
        np.testing.assert_allclose(returns, [1, 1, 1])

    def test_masking_padding_value_range_and_parameter_budget(self):
        batch = spire.TrainingBatch(2, 1, 10, [1000000000, 1000000001])
        inputs = network_inputs(views(batch), self.device, [0, 1])
        self.model.eval()
        with torch.no_grad():
            logits, value = self.model(inputs)
            padded = {name: tensor.clone() for name, tensor in inputs.items()}
            for name in ("tags", "ids", "values"):
                padded[name] = torch.cat((padded[name], torch.ones_like(padded[name][..., :2])), dim=-1)
            padded["entity_mask"] = torch.cat((padded["entity_mask"], torch.zeros(2, 2, dtype=torch.bool)), dim=-1)
            changed_logits, changed_value = self.model(padded)
        torch.testing.assert_close(logits, changed_logits, atol=2e-6, rtol=2e-6)
        torch.testing.assert_close(value, changed_value)
        self.assertTrue(((value > 0) & (value < 1)).all())
        distribution_, entropy = distribution(torch.tensor([[0.0, -torch.inf], [0.0, 0.0]]),
                                               torch.tensor([[True, False], [True, True]]))
        torch.testing.assert_close(entropy, torch.tensor([0.0, 1.0]))
        self.assertEqual(float(distribution_.probs[0, 1]), 0)
        count = sum(p.numel() for p in Policy(ModelConfig()).parameters())
        self.assertTrue(500000 < count < 2000000)
        print(f"default policy parameters: {count}")

    def test_collection_freezes_policy_and_reuses_stored_candidates(self):
        roots = [dict(seed=1000000000 + i, candidates=[]) for i in range(5)]
        collector = Collector(2, 3, 2, self.device, 1)
        before = {key: tensor.clone() for key, tensor in self.model.state_dict().items()}
        rollout = collector.collect(self.model, roots, 1729, range(5), policy_version=7)
        self.assertEqual(rollout.count, 15)
        self.assertEqual(len(rollout.episodes), 5)
        self.assertTrue(all(e["outcome"] == "truncated" for e in rollout.episodes))
        self.assertTrue((rollout.returns[:15] > 0).all())
        self.assertFalse(rollout.complete.any())
        np.testing.assert_array_equal(rollout.policy_versions[:15], np.full(15, 7))
        for key, tensor in self.model.state_dict().items():
            torch.testing.assert_close(before[key], tensor, rtol=0, atol=0)
        padded = spire.ObservationBatch(15)
        padded.pad(rollout.data, rollout.offsets[:16], np.arange(15, dtype=np.int32))
        inputs = network_inputs(views(padded), self.device, slice(None))
        with torch.no_grad():
            logits, value = self.model(inputs)
            policy, _ = distribution(logits, inputs["candidate_mask"])
            old = policy.log_prob(torch.tensor(rollout.actions[:15]))
        np.testing.assert_allclose(old.numpy(), rollout.log_probs[:15], atol=2e-6)
        np.testing.assert_allclose(value.numpy(), rollout.values[:15], atol=2e-6)
        config = PPOConfig(epochs=2, minibatch_size=4)
        result = update(self.model, torch.optim.Adam(self.model.parameters(), lr=config.learning_rate),
                        rollout, config, self.device, np.random.default_rng(1), policy_version=7)
        self.assertEqual(result["samples"], 15)
        self.assertEqual(result["minibatches"], 8)
        self.assertTrue(np.isfinite(result["loss"]))

    def test_fault_is_quarantined_without_losing_other_episode(self):
        roots = [dict(seed=1000000000, candidates=[999]), dict(seed=1000000001, candidates=[])]
        rollout = Collector(2, 1, 1, self.device, 1).collect(self.model, roots, 1729, [0, 1], policy_version=0)
        self.assertEqual([e["outcome"] for e in rollout.episodes], ["fault", "truncated"])
        self.assertEqual(rollout.count, 1)
        self.assertEqual(int(rollout.valid.sum()), 1)
        self.assertEqual(int(rollout.complete.sum()), 0)

    def test_decision_budget_keeps_terminal_targets_and_excludes_unfinished_episode(self):
        roots = [dict(seed=1000000000 + i, candidates=[]) for i in range(2)]
        collector = Collector(2, 4096, 8, self.device, 1)
        full = collector.collect(self.model, roots, 1729, [0, 1], policy_version=0)
        first = min(full.episodes, key=lambda e: e["records"][-1])
        budget = first["records"][-1] + 1
        self.assertLess(budget, full.count)
        partial = collector.collect(self.model, roots, 1729, [0, 1], budget, policy_version=0)
        self.assertEqual(partial.count, budget)
        terminal = next(e for e in partial.episodes if e["seed"] == first["seed"])
        self.assertEqual(terminal["outcome"], first["outcome"])
        self.assertEqual(terminal["candidates"], first["candidates"])
        np.testing.assert_array_equal(partial.returns[terminal["records"]], full.returns[first["records"]])
        unfinished = next(e for e in partial.episodes if e["seed"] != first["seed"])
        self.assertEqual(unfinished["outcome"], "budget_cutoff")
        self.assertFalse(partial.valid[unfinished["records"]].any())
        self.assertTrue(partial.valid[terminal["records"]].all())

    def test_evaluation_stream_is_per_seed_not_batch_order(self):
        roots = [dict(seed=2000000000 + i, candidates=[]) for i in range(3)]
        first = evaluate(self.model, roots, self.device, 5, 2, 2718)
        second = evaluate(self.model, list(reversed(roots)), self.device, 5, 1, 2718)
        a = {e["seed"]: e["candidates"] for e in first["episodes"]}
        b = {e["seed"]: e["candidates"] for e in second["episodes"]}
        self.assertEqual(a, b)
        self.assertIsNone(first["summary"]["wilson95"])
        with self.assertRaises(AssertionError):
            evaluate(self.model, [roots[0], roots[0]], self.device, 5, 1, 2718)
        common = dict(sampling_seed=2718, decision_cap=5, sampling="test", observation_version=2, evaluation_engine_commit="test")
        self.assertEqual(compare(dict(**first, **common), dict(**second, **common))["truncated_pairs"], 3)
        episodes = [dict(seed=1, outcome="victory"), dict(seed=2, outcome="death")]
        self.assertIsNotNone(outcome_summary(episodes)["wilson95"])

    def test_repeated_roots_keep_independent_paired_attempts(self):
        roots = [dict(seed=2000000000, candidates=[])] * 4
        attempts = [11, 12, 13, 14]
        first = evaluate(self.model, roots, self.device, 8, 4, 2718, attempts)
        second = evaluate(self.model, roots, self.device, 8, 1, 2718, list(reversed(attempts)))
        a = {e["attempt"]: e["candidates"] for e in first["episodes"]}
        b = {e["attempt"]: e["candidates"] for e in second["episodes"]}
        self.assertEqual(a, b)
        self.assertGreater(len({tuple(path) for path in a.values()}), 1)
        common = dict(sampling_seed=2718, decision_cap=8, sampling="test", observation_version=2, evaluation_engine_commit="test")
        paired = compare(dict(**first, **common), dict(**second, **common))
        self.assertEqual(paired["truncated_pairs"], 4)
        self.assertEqual(paired["attempt_pairs"], 4)
        self.assertEqual(paired["root_families"], 1)
        repeated = [dict(seed=1, outcome="victory")] * 3
        self.assertEqual(outcome_summary(repeated)["independent_root_families_won"], 1)

    def test_training_inputs_never_call_hidden_state_api(self):
        for path in (ROOT / "train").glob("*.py"):
            tree = ast.parse(path.read_text())
            calls = [node.func.attr for node in ast.walk(tree)
                     if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)]
            self.assertFalse({"snapshot", "search_state", "clone"}.intersection(calls), path)



if __name__ == "__main__":
    unittest.main()
