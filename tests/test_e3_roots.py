import unittest

from tools.e3_roots import BANDS, extract


class E3RootTests(unittest.TestCase):
    def test_only_legal_prefixes_are_cut_from_a_winning_line(self):
        line = dict(seed=1000000100, partition="train", candidates=list(range(600)), stage="heart_turn5", attempt=2,
                    decisions=8000, verification_decisions=600,
                    stage_positions=dict(act3_entry=100, act3_boss=300, act4_entry=500))
        roots = extract([line])
        self.assertEqual([r["band"] for r in roots], list(BANDS))
        for root in roots:
            self.assertEqual(root["candidates"], line["candidates"][:600 - root["distance_to_win"]])
            if root["band"].startswith("k"):
                self.assertEqual(root["distance_to_win"], int(root["band"][1:]))
            self.assertNotIn("winning_candidates", root)
        self.assertEqual(next(r for r in roots if r["band"] == "act4_entry")["distance_to_win"], 100)


if __name__ == "__main__":
    unittest.main()
