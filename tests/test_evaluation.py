import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("evaluation", Path(__file__).resolve().parents[1] / "tools/evaluation.py")
evaluation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evaluation)


class EvaluationTests(unittest.TestCase):
    def test_partitions_do_not_overlap(self):
        intervals = []
        for name in ("train", "dev", "locked_test"):
            for start, end in evaluation.PARTITIONS[name]:
                self.assertEqual(evaluation.partition(start), name)
                self.assertEqual(evaluation.partition(end), name)
                for a, b in intervals:
                    self.assertTrue(end < a or start > b)
                intervals.append((start, end))
        with self.assertRaises(ValueError):
            evaluation.partition(1)

    def test_wilson_known_cases(self):
        low, high = evaluation.wilson(50, 100)
        self.assertAlmostEqual(low, 0.4038315304)
        self.assertAlmostEqual(high, 0.5961684696)
        self.assertAlmostEqual(evaluation.wilson(0, 100)[0], 0)
        self.assertAlmostEqual(evaluation.wilson(100, 100)[1], 1)
        with self.assertRaises(ValueError):
            evaluation.wilson(0, 0)

    def test_pairs_keep_unfinished_roots(self):
        old = dict(enumerate(["victory", "death", "victory", "death", "death", "victory"]))
        new = dict(enumerate(["victory", "death", "death", "victory", "fault", "truncated"]))
        self.assertEqual(evaluation.paired_counts(old, new), dict(roots=6, both_victory=1,
            both_death=1, candidate_only=1, reference_only=1, fault_pairs=1, truncated_pairs=1))
        with self.assertRaises(ValueError):
            evaluation.paired_counts({1: "death"}, {2: "victory"})
