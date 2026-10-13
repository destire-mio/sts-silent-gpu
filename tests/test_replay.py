import json
import tempfile
import types
import unittest
from pathlib import Path

from replay import Difference, compare, replay


class StrictComparison(unittest.TestCase):
    def test_nested_match(self):
        compare({"hand": [{"id": "Strike_G", "damage": 6}]}, {"hand": [{"id": "Strike_G", "damage": 6}]})

    def test_difference_path(self):
        with self.assertRaisesRegex(Difference, r"\$\.hand\[0\]\.damage: expected 6, got 7"):
            compare({"hand": [{"damage": 6}]}, {"hand": [{"damage": 7}]})

    def test_extra_and_missing_fields_fail(self):
        for expected, actual in (({"rng": 1}, {}), ({}, {"rng": 1})):
            with self.subTest(expected=expected):
                with self.assertRaises(Difference):
                    compare(expected, actual)

    def test_array_order_fails(self):
        with self.assertRaises(Difference):
            compare(["Strike_G", "Defend_G"], ["Defend_G", "Strike_G"])

    def test_numeric_type_is_exact(self):
        for value in (True, 1.0, "1"):
            with self.subTest(value=value):
                with self.assertRaises(Difference):
                    compare(1, value)

    def test_no_float_tolerance(self):
        with self.assertRaises(Difference):
            compare(1.0, 1.0000000001)

    def test_nonterminal_snapshot_cannot_end_trace(self):
        class Game:
            def reset(self, seed):
                pass

            def snapshot(self):
                return {"hp": 70}

            def legal_actions(self):
                return [{"type": "end_turn"}]

        records = [
            {"seed": 7, "character": "THE_SILENT", "ascension": 20, "scenario": None},
            {"snapshot": {"hp": 70}, "terminal": "combat_end"},
        ]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trace.jsonl"
            path.write_text("".join(json.dumps(record) + "\n" for record in records))
            with self.assertRaisesRegex(ValueError, "terminal record has legal actions"):
                replay(path, types.SimpleNamespace(Game=Game))

    def test_replay_applies_recorded_actions(self):
        class Game:
            def reset(self, seed):
                self.value = seed

            def snapshot(self):
                return {"value": self.value}

            def legal_actions(self):
                return [{"type": "end_turn"}] if self.value == 7 else []

            def step(self, action):
                compare({"type": "end_turn"}, action)
                self.value += 1

        records = [
            {"seed": 7, "character": "THE_SILENT", "ascension": 20},
            {"snapshot": {"value": 7}, "legal": [{"type": "end_turn"}], "action": {"type": "end_turn"}},
            {"snapshot": {"value": 8}},
        ]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trace.jsonl"
            path.write_text("".join(json.dumps(record) + "\n" for record in records))
            self.assertEqual(1, replay(path, types.SimpleNamespace(Game=Game)))


if __name__ == "__main__":
    unittest.main()
