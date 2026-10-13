import contextlib
import copy
import gzip
import io
import json
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch

from replay import GAME_SHA256, Difference, compare, main, replay, trace_paths


def snapshot(value, finished=False):
    return {
        "value": value,
        "player": {"ref": 0},
        "room": {"ref": 1},
        "objects": [
            {"class": "characters.TheSilent", "fields": {"AbstractCreature.isDead": False}},
            {"class": "rooms.MonsterRoom", "fields": {"AbstractRoom.isBattleOver": finished}},
        ],
        "screen": {"type": "NONE"},
    }


SCENARIO = {"deck": [{"id": "Strike_G", "upgrades": 1}], "relics": [],
            "potions": [None, None], "hp": 50, "max_hp": 66, "encounter": "Cultist"}
ACTION = {"type": "play", "card": 0, "target": 0}


class Game:
    def reset(self, seed):
        self.value = seed

    def start_combat(self, seed, scenario):
        compare(SCENARIO, scenario)
        self.reset(seed)

    def snapshot(self):
        return snapshot(self.value, self.value == 8)

    def legal_actions(self):
        return [ACTION] if self.value == 7 else [{"type": "proceed"}]

    def step(self, action):
        compare(ACTION, action)
        self.value += 1


ENGINE = types.SimpleNamespace(Game=Game)


def records():
    return [
        {"schema": 1, "game_sha256": GAME_SHA256, "seed": 7,
         "character": "THE_SILENT", "ascension": 20, "scenario": copy.deepcopy(SCENARIO)},
        {"snapshot": snapshot(7), "legal": [ACTION], "action": ACTION},
        {"snapshot": snapshot(8, True), "terminal": "combat_end"},
    ]


def write_trace(path, rows):
    content = "".join(json.dumps(record) + "\n" for record in rows).encode("utf-8")
    path.write_bytes(gzip.compress(content, mtime=0) if path.suffix == ".gz" else content)


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

    def test_rng_state_words_are_not_rounded(self):
        with self.assertRaisesRegex(Difference, r"\$\.rng\.aiRng\[1\]"):
            compare({"rng": {"aiRng": [2, 9223372036854775807, -1]}},
                    {"rng": {"aiRng": [2, 9223372036854775806, -1]}})

    def test_object_identity_is_compared(self):
        with self.assertRaisesRegex(Difference, r"\$\.player.ref"):
            compare({"player": {"ref": 0}}, {"player": {"ref": 1}})


class OracleFormat(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.path = self.root / "trace.jsonl"

    def test_scenario_and_recorded_actions_plain_and_gzip(self):
        for name in ("trace.jsonl", "trace.jsonl.gz"):
            with self.subTest(name=name):
                path = self.root / name
                write_trace(path, records())
                self.assertEqual(1, replay(path, ENGINE))

    def test_terminal_can_have_reward_legal_actions(self):
        write_trace(self.path, records())
        self.assertEqual(1, replay(self.path, ENGINE))

    def test_terminal_reason_is_compared(self):
        rows = records()
        rows[-1]["terminal"] = "death"
        write_trace(self.path, rows)
        with self.assertRaisesRegex(Difference, r"\$\.terminal: expected 'death', got 'combat_end'"):
            replay(self.path, ENGINE)

    def test_nonterminal_snapshot_cannot_end_trace(self):
        rows = records()
        rows[1] = {"snapshot": snapshot(7), "terminal": "combat_end"}
        write_trace(self.path, rows[:2])
        with self.assertRaisesRegex(Difference, r"\$\.terminal: expected 'combat_end'.*got None"):
            replay(self.path, ENGINE)

    def test_terminal_is_required(self):
        write_trace(self.path, records()[:-1])
        with self.assertRaisesRegex(ValueError, "terminal snapshot is missing"):
            replay(self.path, ENGINE)

    def test_no_record_after_terminal(self):
        rows = records()
        rows.append(rows[-1])
        write_trace(self.path, rows)
        with self.assertRaisesRegex(ValueError, "record follows terminal snapshot"):
            replay(self.path, ENGINE)

    def test_header_version_and_game_are_checked(self):
        for key, value in (("schema", 2), ("game_sha256", "different"),
                           ("character", "IRONCLAD"), ("ascension", 19)):
            with self.subTest(key=key):
                rows = records()
                rows[0][key] = value
                write_trace(self.path, rows)
                with self.assertRaisesRegex(Difference, r"\$\." + key):
                    replay(self.path, ENGINE)

    def test_full_run_reset_receives_signed_seed(self):
        seed = -9223372036854775808

        class FullRun(Game):
            def reset(self, actual):
                compare(seed, actual)
                self.value = 7

            def snapshot(self):
                result = super().snapshot()
                if self.value == 8:
                    result["screen"]["type"] = "VICTORY"
                return result

        rows = records()
        rows[0].update(seed=seed, scenario=None)
        rows[-1]["snapshot"]["screen"]["type"] = "VICTORY"
        rows[-1]["terminal"] = "victory"
        write_trace(self.path, rows)
        self.assertEqual(1, replay(self.path, types.SimpleNamespace(Game=FullRun)))

    def test_directories_select_combat_but_explicit_file_allows_full_run(self):
        plain = self.root / "a.jsonl"
        zipped = self.root / "b.jsonl.gz"
        full = self.root / "full-run-1.jsonl.gz"
        write_trace(plain, records())
        write_trace(zipped, records())
        rows = records()
        rows[0]["scenario"] = None
        write_trace(full, rows)
        self.assertEqual([plain, zipped], list(trace_paths([self.root])))
        self.assertEqual([full], list(trace_paths([full])))

    def test_first_difference_and_totals_include_matched_decisions(self):
        passed = self.root / "pass.jsonl"
        failed = self.root / "fail.jsonl.gz"
        write_trace(passed, records())
        rows = records()
        rows[-1]["snapshot"]["value"] = 9
        write_trace(failed, rows)
        output = io.StringIO()
        with patch("replay.importlib.import_module", return_value=ENGINE), contextlib.redirect_stdout(output):
            self.assertEqual(1, main([str(self.root)]))
        self.assertIn(f"FAIL {failed}: {failed}:3:$.snapshot.value: expected 9, got 8", output.getvalue())
        self.assertIn(f"PASS {passed}: 1 decisions", output.getvalue())
        self.assertIn("1/2 traces passed; 2 matched decisions", output.getvalue())

    def test_legal_action_order_is_compared_before_step(self):
        rows = records()
        rows[1]["legal"] = [{"type": "end_turn"}]
        write_trace(self.path, rows)
        with self.assertRaisesRegex(Difference, r"\$\.legal\[0\]\.type"):
            replay(self.path, ENGINE)


if __name__ == "__main__":
    unittest.main()
