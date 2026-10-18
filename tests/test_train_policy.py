import ast
from pathlib import Path
import unittest

import numpy as np
import spire
import torch

from train.evaluate import compare, evaluate, outcome_summary
from train.model import ModelConfig, Policy, distribution, network_inputs, packed_inputs, views
from train.ppo import PPOConfig, objective, update
from train.rollout import Collector, Rollout


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
        def episode_targets(rewards, values, bootstrap, gae_lambda):
            n = len(rewards)
            rollout = Rollout.allocate(n, 1)
            rollout.count = n
            rollout.rewards[:] = torch.tensor(rewards)
            rollout.values[:] = torch.as_tensor(values)
            Collector(1, n, 1, self.device, gae_lambda).finish_targets(
                rollout, torch.zeros(n, dtype=torch.int64), torch.arange(n),
                torch.tensor([3 if bootstrap else 1]), torch.tensor([bootstrap]))
            return rollout.returns, rollout.advantages
        values = np.array([0.2, 0.4, 0.8], dtype=np.float32)
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
        self.assertEqual(set(rollout.timing), {"root_prepare_seconds", "reset_seconds", "step_encode_seconds", "inference_seconds",
                                             "storage_seconds", "collection_seconds"})
        self.assertTrue(all(value > 0 for value in rollout.timing.values()))
        self.assertEqual(rollout.count, 15)
        self.assertEqual(len(rollout.episodes), 5)
        self.assertTrue(all(e["outcome"] == "truncated" for e in rollout.episodes))
        self.assertTrue((rollout.returns[:15] > 0).all())
        self.assertFalse(rollout.complete.any())
        np.testing.assert_array_equal(rollout.policy_versions[:15], np.full(15, 7))
        for key, tensor in self.model.state_dict().items():
            torch.testing.assert_close(before[key], tensor, rtol=0, atol=0)
        inputs = packed_inputs(rollout.data, rollout.offsets, torch.arange(15), self.device)
        with torch.no_grad():
            logits, value = self.model(inputs)
            policy, _ = distribution(logits, inputs["candidate_mask"])
            old = policy.log_prob(rollout.actions[:15])
        np.testing.assert_allclose(old.numpy(), rollout.log_probs[:15], atol=2e-6)
        np.testing.assert_allclose(value.numpy(), rollout.values[:15], atol=2e-6)
        config = PPOConfig(epochs=2, minibatch_size=4)
        result = update(self.model, torch.optim.Adam(self.model.parameters(), lr=config.learning_rate),
                        rollout, config, self.device, torch.Generator().manual_seed(1), policy_version=7)
        self.assertEqual(result["samples"], 15)
        self.assertEqual(result["minibatches"], 8)
        self.assertTrue(np.isfinite(result["loss"]))

    def test_root_cache_reuses_prefixes_and_keeps_rollout_bytes(self):
        from reference_collect import winning_root
        roots = [dict(seed=1000000000, candidates=[]), winning_root(8)]
        collector = Collector(2, 4, 2, self.device, .95)
        first = collector.collect(self.model, roots * 2, 1729, range(4), policy_version=0)
        second = collector.collect(self.model, roots * 2, 1729, range(4), policy_version=0)
        self.assertEqual([cache.size for cache in collector.root_caches], [2])
        self.assertEqual(first.episodes, second.episodes)
        for name, value in first.metrics_host().items():
            self.assertEqual(value.tobytes(), second.metrics_host()[name].tobytes(), name)
        length = int(first.offsets[first.count])
        self.assertEqual(first.data[:length].numpy().tobytes(), second.data[:length].numpy().tobytes())
        self.assertEqual(first.offsets[:first.count + 1].numpy().tobytes(), second.offsets[:second.count + 1].numpy().tobytes())
        changed = [roots[1], dict(seed=1000000001, candidates=[]), roots[0]]
        actual = collector.collect(self.model, changed, 1729, range(3), policy_version=0)
        expected = Collector(2, 4, 2, self.device, .95).collect(self.model, changed, 1729, range(3), policy_version=0)
        self.assertEqual([cache.size for cache in collector.root_caches], [2, 1])
        self.assertEqual(actual.episodes, expected.episodes)
        for name, value in actual.metrics_host().items():
            self.assertEqual(value.tobytes(), expected.metrics_host()[name].tobytes(), name)

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

    def test_tensor_collector_matches_scalar_reference(self):
        from reference_collect import ReferenceCollector
        roots = [dict(seed=1000000000 + i, candidates=[]) for i in range(5)]
        roots.insert(2, dict(seed=1000000011, candidates=[999]))
        for cap, budget, lam in ((7, None, 0), (7, None, .95), (7, 19, 1), (4096, 800, .95)):
            with self.subTest(cap=cap, budget=budget, lam=lam):
                args = (self.model, roots, 1729, range(len(roots)), budget)
                expected = ReferenceCollector(3, cap, 8, self.device, lam).collect(*args, policy_version=9)
                actual = Collector(3, cap, 8, self.device, lam).collect(*args, policy_version=9)
                self.assertEqual(actual.count, expected.count)
                for a, b in zip(actual.episodes, expected.episodes):
                    for key in ("seed", "candidates", "records", "outcome", "fault", "policy_version"):
                        self.assertEqual(a[key], b[key], key)
                n = actual.count
                np.testing.assert_array_equal(actual.offsets[:n+1], expected.offsets[:n+1])
                np.testing.assert_array_equal(actual.data[:actual.offsets[n]], expected.data[:expected.offsets[n]])
                for key in ("actions", "rewards", "valid", "complete", "policy_versions"):
                    np.testing.assert_array_equal(getattr(actual, key)[:n], getattr(expected, key)[:n])
                for key in ("values", "log_probs", "returns", "advantages"):
                    mask = expected.valid[:n] if key in ("returns", "advantages") else np.ones(n, bool)
                    np.testing.assert_allclose(getattr(actual, key)[:n][mask], getattr(expected, key)[:n][mask], atol=3e-6)

    def test_terminal_win_and_refill_match_scalar_reference(self):
        from reference_collect import ReferenceCollector, winning_root
        roots = [winning_root(1)] * 5
        a = Collector(2, 4, 2, self.device, .95).collect(self.model, roots, 1729, range(5), policy_version=1)
        b = ReferenceCollector(2, 4, 2, self.device, .95).collect(self.model, roots, 1729, range(5), policy_version=1)
        self.assertEqual([e["outcome"] for e in a.episodes], ["victory"] * 5)
        self.assertEqual([e["candidates"] for e in a.episodes], [e["candidates"] for e in b.episodes])
        np.testing.assert_array_equal(a.returns[:a.count], b.returns[:b.count])
        np.testing.assert_allclose(a.advantages[:a.count], b.advantages[:b.count], atol=3e-6)
        self.assertTrue(a.complete[:a.count].all())
        self.assertTrue(a.valid[:a.count].all())

    def test_post_action_fault_excludes_entire_episode_and_preserves_other_lane(self):
        class FaultBatch:
            def __init__(self):
                self.inner = spire.TrainingBatch(2, 1, 4, [])
                self.faults = np.zeros(2, np.uint32)
                self.calls = 0
            def __getattr__(self, name):
                return getattr(self.inner, name)
            def step_tensors(self, choices, active):
                self.inner.step_tensors(choices, active)
                self.calls += 1
                if self.calls == 2:
                    self.faults[0] = 123
        roots = [dict(seed=1000000000+i, candidates=[]) for i in range(2)]
        reference = Collector(2, 4, 2, self.device, .95).collect(self.model, roots, 1729, [0, 1], policy_version=0)
        actual = Collector(2, 4, 2, self.device, .95, batch=FaultBatch()).collect(self.model, roots, 1729, [0, 1], policy_version=0)
        self.assertEqual(actual.episodes[0]["outcome"], "fault")
        self.assertEqual(actual.episodes[0]["fault"], 123)
        self.assertEqual(len(actual.episodes[0]["records"]), 2)
        self.assertFalse(actual.valid[actual.episodes[0]["records"]].any())
        a, b = actual.episodes[1], reference.episodes[1]
        self.assertEqual(a["candidates"], b["candidates"])
        self.assertEqual(a["outcome"], b["outcome"])
        torch.testing.assert_close(actual.returns[a["records"]], reference.returns[b["records"]])

    def test_inference_chunk_boundary_keeps_episode_identity(self):
        roots = [dict(seed=1000000000+i, candidates=[]) for i in range(259)]
        a = Collector(259, 2, 8, self.device, .95).collect(self.model, roots, 1729, range(259), policy_version=0)
        b = Collector(31, 2, 8, self.device, .95).collect(self.model, roots, 1729, range(259), policy_version=0)
        self.assertEqual([e["candidates"] for e in a.episodes], [e["candidates"] for e in b.episodes])
        self.assertEqual(a.count, 518)

    def test_entity_order_and_chunk_sizes_preserve_rollout(self):
        from reference_collect import winning_root
        roots = [dict(seed=1000000000, candidates=[]), winning_root(8), winning_root(1),
                 dict(seed=1000000001, candidates=[]), dict(seed=1000000020, candidates=[999])] * 3
        for budget in (None, 17):
            a = Collector(7, 4, 8, self.device, .95, inference_chunk=2)
            b = Collector(7, 4, 8, self.device, .95, inference_chunk=5)
            counts = a.arrays["entity_counts"]
            a.arrays["entity_counts"] = torch.tensor([80, 16, 90, 32, 17, 60, 18])
            order = a.inference_order(torch.arange(7))
            self.assertEqual([part.tolist() for part in order], [[1, 4], [6, 3], [5, 0], [2]])
            a.arrays["entity_counts"] = counts
            x = a.collect(self.model, roots, 1729, range(len(roots)), budget, policy_version=7)
            y = b.collect(self.model, roots, 1729, range(len(roots)), budget, policy_version=7)
            self.assertEqual(x.count, y.count)
            for left, right in zip(x.episodes, y.episodes):
                self.assertEqual({k: v for k, v in left.items() if k != "bootstrap"},
                                 {k: v for k, v in right.items() if k != "bootstrap"})
            for name in ("offsets", "actions", "rewards", "screens", "acts", "complete", "valid",
                         "policy_versions", "global_ids"):
                n = x.count + (name == "offsets")
                torch.testing.assert_close(getattr(x, name)[:n], getattr(y, name)[:n], atol=0, rtol=0)
            torch.testing.assert_close(x.data[:x.offsets[x.count]], y.data[:y.offsets[y.count]], atol=0, rtol=0)
            for name in ("values", "log_probs", "returns", "advantages", "entropy"):
                torch.testing.assert_close(getattr(x, name)[:x.count], getattr(y, name)[:y.count], atol=3e-6, rtol=3e-6)

    def test_counter_matches_independent_integer_reference_and_permutation(self):
        from reference_collect import word
        from train.sampling import random_words, root_key, root_words
        roots = [dict(seed=-17, candidates=[1]), dict(seed=-17, candidates=[0]), dict(seed=2**62, candidates=[])]
        keys = root_words(roots, self.device)
        attempts = torch.tensor([2**33, 7, 14])
        decisions = torch.tensor([2**34, 9, 33])
        actual = random_words(2**45+1729, keys, attempts, decisions)
        expected = [word(2**45+1729, root_key(r), int(a), int(d)) for r, a, d in zip(roots, attempts, decisions)]
        self.assertEqual(actual.tolist(), expected)
        permutation = torch.tensor([2, 0, 1])
        torch.testing.assert_close(random_words(2**45+1729, keys[permutation], attempts[permutation], decisions[permutation]), actual[permutation], atol=0, rtol=0)

    def test_training_inputs_never_call_hidden_state_api(self):
        for path in (ROOT / "train").glob("*.py"):
            tree = ast.parse(path.read_text())
            calls = [node.func.attr for node in ast.walk(tree)
                     if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)]
            self.assertFalse({"snapshot", "search_state"}.intersection(calls), path)



if __name__ == "__main__":
    unittest.main()
