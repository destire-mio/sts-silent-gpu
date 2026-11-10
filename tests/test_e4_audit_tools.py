from unittest import TestCase

from tools.e4_card_horizons import heldout_gain, reached_horizon
from tools.e4_map_labels import gold_result, stratified_roots
from tools.e4_value_residual import summarize, training_seeds
from train.e4 import RewardLedger


class AuditToolTests(TestCase):
    def test_residual_support_and_training_seeds(self):
        self.assertEqual(training_seeds(["1000000000:1000000002"]), [1000000000, 1000000001])
        with self.assertRaises(ValueError):
            training_seeds(["2000000000"])
        samples = [
            dict(cards=([1, 2] if i < 29 else [1]) if i < 30 else [],
                 residual=1. if i < 30 else -2., type_counts={"Power": int(i < 30)})
            for i in range(31)
        ]
        report = summarize([{"milestones": {"enter_act2": row}} for row in samples])["enter_act2"]
        self.assertEqual(list(report["cards"]), [1])
        self.assertEqual(report["cards"][1]["present"]["samples"], 30)
        self.assertEqual(report["cards"][1]["absent"]["mean"], -2.)
        self.assertEqual(report["powers"]["1"]["mean"], 1.)

    def test_map_selection_cap_and_gold_holdout(self):
        episodes = [dict(seed=seed, path=[0, 1, 2],
                         roots=[dict(act=1, floor=floor, prefix_length=floor) for floor in range(3)])
                    for seed in (1000000000, 1000000001)]
        roots = stratified_roots(episodes, 4, 101)
        self.assertEqual(len({root["floor"] for root in roots}), 3)
        for episode in episodes:
            self.assertEqual(sum(root["seed"] == episode["seed"] for root in roots), 2)
        result = gold_result({0: [.2] * 128, 1: [.6] * 64 + [.1] * 64}, 0)
        self.assertEqual(result["selected"], 1)
        self.assertAlmostEqual(result["regret"], -.1)

    def test_card_holdout_and_terminal_boundaries(self):
        result = heldout_gain({0: [2., 2., 2., 2.], 1: [3., 3., 1., 1.]}, 0)
        self.assertEqual(result["gain"], -1.)
        ledger = RewardLedger()
        ledger.reach["act3_first_boss"] = True
        self.assertFalse(reached_horizon("next_act", 2, 3, ledger, False))
        ledger.reach["act3_second_boss"] = True
        self.assertTrue(reached_horizon("next_act", 2, 3, ledger, False))
        self.assertFalse(reached_horizon("full", 2, 3, ledger, False))
        self.assertTrue(reached_horizon("full", 2, 3, ledger, True))
