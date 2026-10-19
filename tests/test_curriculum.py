from collections import Counter
from dataclasses import asdict
import gzip
import json
from pathlib import Path
import tempfile
import time
from types import SimpleNamespace
import unittest

import torch

from train import OBSERVATION_VERSION
from train.curriculum import (BOOTSTRAP_SAMPLES, BOOTSTRAP_SEED, Curriculum, assess,
                              family_statistics, paired_statistics, select_bands)
from train.model import ModelConfig
from train.pipeline import Pipeline, PipelineConfig
from train.ppo import PPOConfig
from train.sampling import root_key


def roots():
    train = [dict(seed=1000000000 + i, candidates=[], band=band) for band in ("k8", "k32") for i in range(20)]
    far = [dict(seed=1000001000 + i, candidates=[]) for i in range(20)]
    return train, far


def statistics(lower, mean):
    return dict(interval95=[0., 1.], mean=mean, families=30, bootstrap_lower95=lower,
                bootstrap_seed=BOOTSTRAP_SEED, bootstrap_samples=BOOTSTRAP_SAMPLES)


def episodes(families, wins):
    return [dict(seed=2000000000 + i, outcome="victory" if i < wins else "death") for i in range(families)]


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
        curriculum.assess(statistics(.8, .9))
        group, sources = curriculum.draw(10000)
        self.assertEqual(Counter(sources), {"frontier": 6000, "learned": 2000, "far": 2000})
        self.assertTrue(all(r["band"] == "k8" for r, s in zip(group, sources) if s == "learned"))
        train, far = roots()
        with self.assertRaisesRegex(AssertionError, "5%"):
            Curriculum(train[1:], far, ["k8", "k32"], 1)

    def test_promotion_uses_strict_family_lower_bound_and_stagnation(self):
        curriculum = self.make()
        curriculum.assess(statistics(.6, .9))
        self.assertEqual(curriculum.frontier, "k8")
        curriculum.assess(statistics(.600001, .9))
        self.assertEqual(curriculum.frontier, "k32")
        self.assertEqual(curriculum.state["learned"], ["k8"])
        for _ in range(4):
            curriculum.assess(statistics(.5, .7))
        self.assertEqual(curriculum.state["stopped"], "three_evaluations_without_frontier_improvement")

    def test_state_and_next_draws_survive_checkpoint(self):
        curriculum = self.make()
        _, sources = curriculum.draw(137)
        episodes = [dict(attempt=i, records=list(range(i % 5 + 1)), outcome="death") for i in range(137)]
        curriculum.record(episodes, sources, 0)
        curriculum.assess(statistics(.8, .9))
        snapshot = curriculum.state_dict()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.pt"
            torch.save(snapshot, path)
            loaded = torch.load(path, weights_only=True)
        self.assertEqual(snapshot, loaded)
        self.assertEqual(loaded["bootstrap_seed"], BOOTSTRAP_SEED)
        self.assertEqual(loaded["bootstrap_samples"], 10000)
        restored = self.make(loaded)
        self.assertEqual(curriculum.draw(1000), restored.draw(1000))
        self.assertEqual(curriculum.state_dict(), restored.state_dict())

    def test_statistics_weight_families_and_pair_by_seed(self):
        episodes = [dict(seed=2000000000, outcome="victory")] * 100
        episodes += [dict(seed=2000000001, outcome="death")]
        summary = family_statistics(episodes)
        self.assertEqual(summary["mean"], .5)
        self.assertEqual(summary["families"], 2)
        self.assertEqual(paired_statistics(summary, summary)["mean_change"], 0)
        with self.assertRaises(AssertionError):
            family_statistics([dict(seed=3000000000, outcome="victory")])

    def test_seven_perfect_dev_families_cannot_promote(self):
        curriculum = self.make()
        summary = family_statistics(episodes(7, 7))
        self.assertEqual(summary["bootstrap_lower95"], 1.)
        event = curriculum.assess(summary)
        self.assertFalse(event["eligible"])
        self.assertEqual(curriculum.frontier, "k8")

    def test_ten_perfect_dev_families_promote(self):
        curriculum = self.make()
        summary = family_statistics(episodes(10, 10))
        self.assertEqual(summary["bootstrap_lower95"], 1.)
        event = curriculum.assess(summary)
        self.assertTrue(event["eligible"])
        self.assertEqual(curriculum.frontier, "k32")

    def test_ten_families_at_eighty_percent_do_not_promote(self):
        curriculum = self.make()
        summary = family_statistics(episodes(10, 8))
        self.assertEqual(summary["bootstrap_lower95"], .6)
        event = curriculum.assess(summary)
        self.assertTrue(event["eligible"])
        self.assertEqual(curriculum.frontier, "k8")

    def test_thirty_families_at_eighty_percent_promote(self):
        curriculum = self.make()
        summary = family_statistics(episodes(30, 24))
        self.assertGreater(summary["bootstrap_lower95"], .6)
        curriculum.assess(summary)
        self.assertEqual(curriculum.frontier, "k32")

    def test_thirty_families_at_sixty_percent_do_not_promote(self):
        curriculum = self.make()
        summary = family_statistics(episodes(30, 18))
        self.assertLessEqual(summary["bootstrap_lower95"], .6)
        curriculum.assess(summary)
        self.assertEqual(curriculum.frontier, "k8")

    def test_family_bootstrap_is_reproducible_and_pairs_by_seed(self):
        before = family_statistics(episodes(30, 0))
        after = family_statistics(episodes(30, 24))
        reordered = family_statistics(list(reversed(episodes(30, 24))))
        self.assertEqual(after, reordered)
        paired = paired_statistics(before, after)
        self.assertEqual(paired, paired_statistics(before, reordered))
        self.assertEqual(paired["bootstrap_lower95"], after["bootstrap_lower95"])
        self.assertGreater(paired["bootstrap_lower95"], 0.)
        self.assertEqual(paired_statistics(after, after)["bootstrap_lower95"], 0.)

    def test_band_selection_requires_twenty_train_and_ten_dev_families(self):
        train = [dict(seed=1000000000 + i, band=band, distance_to_win=distance)
                 for band, distance in (("k8", 8), ("k32", 32), ("k64", 64), ("k128", 128))
                 for i in range(20)]
        dev = [dict(seed=2000000000 + i, band=band) for band in ("k8", "k32", "k64", "k128")
               for i in range(10 if band != "k8" else 9)]
        bands, counts = select_bands(train, dev)
        self.assertEqual(bands, ["k32", "k64", "k128"])
        self.assertLess(len(bands), 4)
        self.assertEqual(counts["k8"], {"train": 20, "dev": 9})

    def test_assessment_preserves_fault_reproduction_before_aborting(self):
        root = dict(seed=1000000000, candidates=[1, 2])
        episode = dict(seed=root["seed"], root_key=root_key(root), attempt=0, candidates=[3], records=[0],
                       outcome="fault", fault=17)
        pipeline = SimpleNamespace(config=SimpleNamespace(workers=1), version=0,
            evaluate=lambda *_: dict(episodes=[episode], decisions=1, seconds=0., memory=[dict(rank=0)]))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            with self.assertRaisesRegex(AssertionError, "Evaluation engine fault"):
                assess(pipeline, {"train/k8": [root]}, output, "initial", 0, {}, time.time() + 3600)
            saved = json.loads((output / "fault-initial-train-k8-0.json").read_text())
            self.assertEqual(saved[0]["root"], root)
            self.assertEqual(saved[0]["episode"]["candidates"], [3])
            self.assertEqual(saved[0]["episode"]["fault"], 17)

    def test_assessment_writes_statistics_and_complete_policy_paths(self):
        root = dict(seed=1000000000, candidates=[1, 2])
        episodes = [dict(seed=root["seed"], root_key=root_key(root), attempt=i, candidates=[3], records=[i],
                         outcome="death", fault=0) for i in range(4)]
        pipeline = SimpleNamespace(config=SimpleNamespace(workers=1), version=0,
            evaluate=lambda *_: dict(episodes=episodes, decisions=4, seconds=.1, memory=[dict(rank=0, peak_allocated_bytes=128)]))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            statistics, decisions, seconds, complete = assess(
                pipeline, {"train/k8": [root]}, output, "initial", 0, {}, time.time() + 3600)
            self.assertTrue(complete)
            self.assertEqual((decisions, seconds), (4, .1))
            self.assertEqual(statistics["train/k8"]["attempts"], 4)
            record = json.loads((output / "learning-curve.jsonl").read_text())
            self.assertEqual(record["peak_allocated_bytes_by_rank"], [128])
            with gzip.open(output / "eval-initial-train-k8.jsonl.gz", "rt") as stream:
                paths = [json.loads(line) for line in stream]
            self.assertEqual([e["candidates"] for e in paths], [[3]] * 4)
            self.assertTrue(all("records" not in e for e in paths))

    def test_pipeline_evaluation_and_curriculum_resume(self):
        torch.set_num_threads(1)
        model = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
        ppo = PPOConfig(epochs=1, minibatch_size=8)
        config = PipelineConfig(workers=2, device="cpu", batch_size=2, decision_cap=3, storage_mib=4)
        manifest = dict(engine_commit="test", model=asdict(model), observation_version=OBSERVATION_VERSION)
        train, _ = roots()
        group = train[:4]
        curriculum = self.make()
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / "first", Path(directory) / "second"
            first.mkdir()
            second.mkdir()
            with Pipeline(model, ppo, config, 1729, first, manifest) as pipeline:
                pipeline.initialize()
                pipeline.save_curriculum(curriculum.state_dict())
                self.assertEqual(torch.load(first / "checkpoint.pt", weights_only=True)["curriculum"], curriculum.state_dict())
                before = pipeline.state
                evaluated = pipeline.evaluate(group, 63100, range(4))
                self.assertEqual(evaluated["decisions"], 12)
                self.assertIsNone(pipeline.pending)
                self.assertEqual(pipeline.version, 0)
                for name, value in pipeline.state.items():
                    torch.testing.assert_close(value, before[name], atol=0, rtol=0)
                rollout = pipeline.collect(group, 1729, range(4))
                state = curriculum.state_dict()
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
