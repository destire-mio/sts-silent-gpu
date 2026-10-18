from collections import Counter
from dataclasses import asdict
from pathlib import Path
import tempfile
import unittest

import torch

from train.curriculum import Curriculum, family_statistics, paired_statistics
from train.model import ModelConfig
from train.pipeline import Pipeline, PipelineConfig
from train.ppo import PPOConfig


def roots():
    train = [dict(seed=1000000000 + i, candidates=[], band=band) for band in ("k8", "k32") for i in range(20)]
    far = [dict(seed=1000001000 + i, candidates=[]) for i in range(20)]
    return train, far


class CurriculumTests(unittest.TestCase):
    def make(self, state=None):
        train, far = roots()
        return Curriculum(train, far, ["k8", "k32"], 1729, state)

    def test_sampling_proportions_and_family_cap(self):
        curriculum = self.make()
        group, sources = curriculum.draw(10000)
        self.assertEqual(Counter(sources), {"frontier": 8000, "far": 2000})
        for source in set(sources):
            counts = Counter(r["seed"] for r, s in zip(group, sources) if s == source)
            self.assertLessEqual(max(counts.values()) / sum(counts.values()), .05)
        curriculum.assess(dict(interval95=[.8, 1.], mean=.9))
        group, sources = curriculum.draw(10000)
        self.assertEqual(Counter(sources), {"frontier": 6000, "learned": 2000, "far": 2000})
        self.assertTrue(all(r["band"] == "k8" for r, s in zip(group, sources) if s == "learned"))
        train, far = roots()
        with self.assertRaisesRegex(AssertionError, "5%"):
            Curriculum(train[1:], far, ["k8", "k32"], 1)

    def test_promotion_uses_strict_family_lower_bound_and_stagnation(self):
        curriculum = self.make()
        curriculum.assess(dict(interval95=[.6, .99], mean=.9))
        self.assertEqual(curriculum.frontier, "k8")
        curriculum.assess(dict(interval95=[.600001, .99], mean=.9))
        self.assertEqual(curriculum.frontier, "k32")
        self.assertEqual(curriculum.state["learned"], ["k8"])
        for _ in range(4):
            curriculum.assess(dict(interval95=[.5, .9], mean=.7))
        self.assertEqual(curriculum.state["stopped"], "three_evaluations_without_frontier_improvement")

    def test_state_and_next_draws_survive_checkpoint(self):
        curriculum = self.make()
        _, sources = curriculum.draw(137)
        episodes = [dict(attempt=i, records=list(range(i % 5 + 1))) for i in range(137)]
        curriculum.record(episodes, sources, 0)
        curriculum.assess(dict(interval95=[.8, 1.], mean=.9))
        snapshot = curriculum.snapshot()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.pt"
            torch.save(snapshot, path)
            loaded = torch.load(path, weights_only=True)
        self.assertEqual(snapshot, loaded)
        restored = self.make(loaded)
        self.assertEqual(curriculum.draw(1000), restored.draw(1000))
        self.assertEqual(curriculum.snapshot(), restored.snapshot())

    def test_statistics_weight_families_and_pair_by_seed(self):
        episodes = [dict(seed=2000000000, outcome="victory")] * 100
        episodes += [dict(seed=2000000001, outcome="death")]
        summary = family_statistics(episodes)
        self.assertEqual(summary["mean"], .5)
        self.assertEqual(summary["families"], 2)
        self.assertEqual(paired_statistics(summary, summary)["mean_change"], 0)
        with self.assertRaises(AssertionError):
            family_statistics([dict(seed=3000000000, outcome="victory")])

    def test_pipeline_evaluation_and_curriculum_resume(self):
        torch.set_num_threads(1)
        model = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
        ppo = PPOConfig(epochs=1, minibatch_size=8)
        config = PipelineConfig(workers=2, device="cpu", batch_size=2, decision_cap=3, storage_mib=4)
        manifest = dict(engine_commit="test", model=asdict(model), observation_version=2)
        train, _ = roots()
        group = train[:4]
        curriculum = self.make()
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / "first", Path(directory) / "second"
            first.mkdir()
            second.mkdir()
            with Pipeline(model, ppo, config, 1729, first, manifest) as pipeline:
                pipeline.initialize()
                before = pipeline.state
                evaluated = pipeline.evaluate(group, 63100, range(4))
                self.assertEqual(evaluated["decisions"], 12)
                self.assertIsNone(pipeline.pending)
                self.assertEqual(pipeline.version, 0)
                for name, value in pipeline.state.items():
                    torch.testing.assert_close(value, before[name], atol=0, rtol=0)
                rollout = pipeline.collect(group, 1729, range(4))
                state = curriculum.snapshot()
                pipeline.update(rollout, curriculum=state)
                saved = torch.load(first / "checkpoint.pt", weights_only=True)
                self.assertEqual(saved["curriculum"], state)
                expected = pipeline.collect(group, 1729, range(4, 8))
                expected_candidates = [e["candidates"] for e in expected.episodes]
                pipeline.update(expected, curriculum=state)
                expected_state = pipeline.state
            with Pipeline(model, ppo, config, 1729, second, manifest, saved) as pipeline:
                pipeline.initialize()
                self.assertEqual(torch.load(second / "checkpoint.pt", weights_only=True)["curriculum"], state)
                actual = pipeline.collect(group, 1729, range(4, 8))
                self.assertEqual([e["candidates"] for e in actual.episodes], expected_candidates)
                pipeline.update(actual, curriculum=state)
                for name, value in pipeline.state.items():
                    torch.testing.assert_close(value, expected_state[name], atol=0, rtol=0)


if __name__ == "__main__":
    unittest.main()
