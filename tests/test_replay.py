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

from replay import COMBAT_RNG, GAME_SHA256, Difference, compare, compare_scope, main, replay, trace_paths


def snapshot(value, finished=False):
    return {
        "run": {"value": value},
        "player": {"ref": 0},
        "room": {"ref": 1},
        "manager": {"ref": 2},
        "combat": {"value": value},
        "map": [], "pools": {}, "globals": {}, "node": None, "next_node": None,
        "objects": [
            {"class": "characters.TheSilent", "fields": {"AbstractCreature.isDead": False}},
            {"class": "rooms.MonsterRoom", "fields": {
                "AbstractRoom.isBattleOver": finished, "AbstractRoom.monsters": {"ref": 3}}},
            {"class": "actions.GameActionManager", "fields": {}},
            {"class": "monsters.MonsterGroup", "fields": {"MonsterGroup.monsters": []}},
        ],
        "screen": {"type": "NONE", "up": False, "previous": None, "choice": "COMBAT",
                   "options": [], "state": None, "room_dialog": {}, "image_dialog": {}},
        "rng": {name: [0, 2**63 - 1, -1] for name in COMBAT_RNG | {"mapRng"}},
    }


SCENARIO = {"deck": [{"id": "Strike_G", "upgrades": 1}], "relics": [],
            "potions": [None, None], "hp": 50, "max_hp": 66, "encounter": "Cultist"}
ACTION = {"type": "play", "card": 0, "target": 0}


class Game:
    def reset(self, seed, playtime_at_least_800=False):
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
        {"schema": 2, "game_sha256": GAME_SHA256, "seed": 7,
         "character": "THE_SILENT", "ascension": 20, "scenario": copy.deepcopy(SCENARIO)},
        {"snapshot": snapshot(7), "legal": [ACTION], "action": ACTION},
        {"snapshot": snapshot(8, True), "terminal": "combat_end"},
    ]


def write_trace(path, rows):
    content = "".join(json.dumps(record) + "\n" for record in rows).encode("utf-8")
    path.write_bytes(gzip.compress(content, mtime=0) if path.suffix == ".gz" else content)


def reindex(value, mapping):
    if isinstance(value, dict):
        if set(value) == {"ref"}:
            return {"ref": mapping[value["ref"]]}
        return {key: reindex(item, mapping) for key, item in value.items()}
    if isinstance(value, list):
        return [reindex(item, mapping) for item in value]
    return value


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


class GraphScopes(unittest.TestCase):
    def test_reindexed_graph_and_cycles_match(self):
        expected = snapshot(7)
        expected["objects"][0]["fields"]["owner"] = {"ref": 0}
        expected["objects"][2]["fields"]["player"] = {"ref": 0}
        mapping = {0: 2, 1: 0, 2: 3, 3: 1}
        actual = reindex(expected, mapping)
        actual["objects"] = [actual["objects"][i] for i in (1, 3, 0, 2)]
        for scope in ("combat", "run"):
            compare_scope(expected, actual, scope)

    def test_shared_reference_cannot_be_cloned(self):
        expected = snapshot(7)
        expected["objects"][2]["fields"]["player"] = {"ref": 0}
        actual = copy.deepcopy(expected)
        actual["objects"].append(copy.deepcopy(actual["objects"][0]))
        actual["objects"][2]["fields"]["player"] = {"ref": 4}
        with self.assertRaisesRegex(Difference, "identity"):
            compare_scope(expected, actual, "combat")
        with self.assertRaisesRegex(Difference, "identity"):
            compare_scope(actual, expected, "combat")

    def test_missing_card_field_fails(self):
        expected = snapshot(7)
        expected["objects"][0]["fields"]["hand"] = {"ref": 4}
        expected["objects"].append({"class": "cards.green.Strike_G", "fields": {
            "AbstractCard.damage": 6, "AbstractCard.uuid": 0}})
        actual = copy.deepcopy(expected)
        del actual["objects"][4]["fields"]["AbstractCard.damage"]
        with self.assertRaisesRegex(Difference, r"player.fields.hand.fields.AbstractCard.damage.*<missing>"):
            compare_scope(expected, actual, "combat")

    def test_uuid_labels_preserve_equality_between_distinct_card_objects(self):
        expected = snapshot(7)
        expected["objects"][0]["fields"]["cards"] = [{"ref": 4}, {"ref": 5}]
        card = {"class": "cards.green.Strike_G", "fields": {"AbstractCard.uuid": 0}}
        expected["objects"].extend([copy.deepcopy(card), copy.deepcopy(card)])
        actual = copy.deepcopy(expected)
        for index in (4, 5):
            actual["objects"][index]["fields"]["AbstractCard.uuid"] = 9
        compare_scope(expected, actual, "combat")
        actual["objects"][5]["fields"]["AbstractCard.uuid"] = 10
        with self.assertRaisesRegex(Difference, "AbstractCard.uuid.*identity"):
            compare_scope(expected, actual, "combat")

    def test_monsters_are_combat_and_room_rewards_are_run(self):
        expected = snapshot(7)
        actual = copy.deepcopy(expected)
        actual["objects"][3]["fields"]["MonsterGroup.monsters"] = [None]
        compare_scope(expected, actual, "run")
        with self.assertRaises(Difference):
            compare_scope(expected, actual, "combat")
        actual = copy.deepcopy(expected)
        actual["objects"][1]["fields"]["AbstractRoom.rewards"] = []
        compare_scope(expected, actual, "combat")
        with self.assertRaises(Difference):
            compare_scope(expected, actual, "run")

    def test_hand_select_fields_are_complete_combat_state(self):
        expected = snapshot(7)
        expected["screen"].update(type="HAND_SELECT", state={"ref": 4})
        expected["objects"].append({"class": "screens.select.HandCardSelectScreen", "fields": {
            "HandCardSelectScreen.numCardsToSelect": 1}})
        actual = copy.deepcopy(expected)
        actual["objects"][4]["fields"]["HandCardSelectScreen.numCardsToSelect"] = 2
        compare_scope(expected, actual, "run")
        with self.assertRaisesRegex(Difference, "numCardsToSelect"):
            compare_scope(expected, actual, "combat")

    def test_reward_state_is_run_but_options_are_combat(self):
        expected = snapshot(7)
        expected["screen"].update(type="COMBAT_REWARD", state={"ref": 4}, options=["GOLD"])
        expected["objects"].append({"class": "screens.CombatRewardScreen", "fields": {"goldAmt": 10}})
        actual = copy.deepcopy(expected)
        actual["objects"][4]["fields"]["goldAmt"] = 20
        compare_scope(expected, actual, "combat")
        with self.assertRaisesRegex(Difference, "goldAmt"):
            compare_scope(expected, actual, "run")
        actual["screen"]["options"] = []
        with self.assertRaisesRegex(Difference, "options"):
            compare_scope(expected, actual, "combat")

    def test_each_rng_is_in_exactly_one_scope(self):
        expected = snapshot(7)
        for stream in expected["rng"]:
            with self.subTest(stream=stream):
                actual = copy.deepcopy(expected)
                actual["rng"][stream][1] -= 1
                scope = "combat" if stream in COMBAT_RNG else "run"
                with self.assertRaisesRegex(Difference, stream):
                    compare_scope(expected, actual, scope)
                compare_scope(expected, actual, "run" if scope == "combat" else "combat")

    def test_cross_scope_card_identity_is_checked(self):
        expected = snapshot(7)
        expected["globals"]["owner"] = {"ref": 0}
        actual = copy.deepcopy(expected)
        actual["objects"].append(copy.deepcopy(actual["objects"][0]))
        actual["globals"]["owner"] = {"ref": 4}
        identities = ({}, {}, {}, {})
        compare_scope(expected, actual, "combat", identities=identities)
        with self.assertRaisesRegex(Difference, "identity"):
            compare_scope(expected, actual, "run", identities=identities)


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
                result = replay(path, ENGINE)
                self.assertTrue(result.passed)
                self.assertEqual(1, result.decisions)
                self.assertEqual(1, result.scopes["combat"].matched_decisions)

    def test_terminal_can_have_reward_legal_actions(self):
        write_trace(self.path, records())
        self.assertTrue(replay(self.path, ENGINE).passed)

    def test_terminal_reason_is_compared(self):
        rows = records()
        rows[-1]["terminal"] = "death"
        write_trace(self.path, rows)
        result = replay(self.path, ENGINE)
        self.assertFalse(result.passed)
        self.assertIn("$.terminal: expected 'death', got 'combat_end'", result.scopes["combat"].differences[0])

    def test_nonterminal_snapshot_cannot_end_trace(self):
        rows = records()
        rows[1] = {"snapshot": snapshot(7), "terminal": "combat_end"}
        write_trace(self.path, rows[:2])
        result = replay(self.path, ENGINE)
        self.assertFalse(result.passed)
        self.assertRegex(result.scopes["combat"].differences[0], r"\$\.terminal: expected 'combat_end'.*got None")

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
        for key, value in (("schema", 1), ("game_sha256", "different"),
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
            def reset(self, actual, playtime_at_least_800=False):
                compare(seed, actual)
                compare(True, playtime_at_least_800)
                self.value = 7

            def snapshot(self):
                result = super().snapshot()
                if self.value == 8:
                    result["screen"]["type"] = "VICTORY"
                return result

        rows = records()
        rows[0].update(seed=seed, scenario=None, playtime_at_least_800=True)
        rows[-1]["snapshot"]["screen"]["type"] = "VICTORY"
        rows[-1]["terminal"] = "victory"
        write_trace(self.path, rows)
        self.assertTrue(replay(self.path, types.SimpleNamespace(Game=FullRun)).passed)

    def test_directories_include_scenarios_and_full_runs(self):
        plain = self.root / "a.jsonl"
        zipped = self.root / "b.jsonl.gz"
        full = self.root / "full-run-1.jsonl.gz"
        write_trace(plain, records())
        write_trace(zipped, records())
        rows = records()
        rows[0]["scenario"] = None
        write_trace(full, rows)
        self.assertEqual([plain, zipped, full], list(trace_paths([self.root])))
        self.assertEqual([full], list(trace_paths([full])))

    def test_first_difference_and_per_scope_totals(self):
        passed = self.root / "pass.jsonl"
        failed = self.root / "fail.jsonl.gz"
        write_trace(passed, records())
        rows = records()
        rows[-1]["snapshot"]["combat"]["value"] = 9
        write_trace(failed, rows)
        output = io.StringIO()
        with patch("replay.importlib.import_module", return_value=ENGINE), contextlib.redirect_stdout(output):
            self.assertEqual(1, main([str(self.root)]))
        self.assertIn(f"FAIL combat {failed}: 1/1 decisions: {failed}:3:$.snapshot.combat.value: expected 9, got 8", output.getvalue())
        self.assertIn(f"PASS combat {passed}: 1/1 decisions", output.getvalue())
        self.assertIn("combat: 1/2 traces passed, 2/2 matched decisions; run: 2/2 traces passed, 2/2 matched decisions", output.getvalue())

    def test_run_failure_does_not_hide_combat_but_fails_scenario(self):
        rows = records()
        rows[1]["snapshot"]["run"]["value"] = 99
        rows[-1]["snapshot"]["run"]["value"] = 99
        write_trace(self.path, rows)
        result = replay(self.path, ENGINE)
        self.assertFalse(result.passed)
        self.assertTrue(result.scopes["combat"].passed)
        self.assertEqual(2, len(result.scopes["run"].differences))
        output = io.StringIO()
        with patch("replay.importlib.import_module", return_value=ENGINE), contextlib.redirect_stdout(output):
            self.assertEqual(1, main([str(self.path)]))
        self.assertIn("combat: 1/1 traces passed, 1/1 matched decisions; run: 0/1 traces passed, 0/1 matched decisions", output.getvalue())

    def test_full_run_filename_has_no_failure_exemption(self):
        path = self.root / "full-run-1.jsonl.gz"
        write_trace(path, records())
        with patch("replay.importlib.import_module", return_value=ENGINE):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(0, main([str(path)]))
            self.assertIn("combat: 1/1 traces passed", output.getvalue())
            self.assertIn("run: 1/1 traces passed", output.getvalue())
            rows = records()
            rows[1]["action"] = {"type": "choose", "option": 0}
            write_trace(path, rows)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(1, main([str(path)]))
            self.assertIn("combat: 0/1 traces passed", output.getvalue())
            self.assertNotIn("XFAIL", output.getvalue())

    def test_full_run_requires_both_scopes(self):
        class FullRun(Game):
            def snapshot(self):
                result = super().snapshot()
                if self.value == 8:
                    result["screen"]["type"] = "VICTORY"
                return result

        rows = records()
        rows[0]["scenario"] = None
        rows[1]["snapshot"]["run"]["value"] = 99
        rows[-1]["snapshot"]["screen"]["type"] = "VICTORY"
        rows[-1]["terminal"] = "victory"
        write_trace(self.path, rows)
        result = replay(self.path, types.SimpleNamespace(Game=FullRun))
        self.assertTrue(result.scopes["combat"].passed)
        self.assertFalse(result.passed)

    def test_matching_later_decision_is_counted_after_difference(self):
        class TwoActions(Game):
            def snapshot(self):
                return snapshot(self.value, self.value == 9)

            def legal_actions(self):
                return [ACTION]

        rows = records()
        rows[1]["snapshot"]["combat"]["value"] = 99
        rows.insert(2, {"snapshot": snapshot(8), "legal": [ACTION], "action": ACTION})
        rows[-1]["snapshot"] = snapshot(9, True)
        write_trace(self.path, rows)
        result = replay(self.path, types.SimpleNamespace(Game=TwoActions))
        self.assertFalse(result.passed)
        self.assertEqual(2, result.decisions)
        self.assertEqual(1, result.scopes["combat"].matched_decisions)
        self.assertTrue(result.scopes["combat"].terminal_matches)
        self.assertTrue(result.scopes["run"].passed)

    def test_legal_action_order_is_compared_before_step(self):
        rows = records()
        rows[1]["legal"] = [{"type": "end_turn"}]
        write_trace(self.path, rows)
        result = replay(self.path, ENGINE)
        self.assertFalse(result.passed)
        self.assertIn("$.legal[0].type", result.scopes["combat"].differences[0])
        self.assertTrue(result.scopes["combat"].terminal_matches)

    def test_unavailable_recorded_action_stops_before_engine_assertion(self):
        rows = records()
        rows[1]["action"] = {"type": "end_turn"}
        write_trace(self.path, rows)
        result = replay(self.path, ENGINE)
        self.assertFalse(result.passed)
        self.assertIn("is not legal in the engine", result.stopped)
        self.assertFalse(result.scopes["combat"].terminal_matches)


if __name__ == "__main__":
    unittest.main()
