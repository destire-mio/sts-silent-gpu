import json
import random
import tempfile
import unittest
from pathlib import Path

import spire
from replay import replay


class HostInterface(unittest.TestCase):
    def test_snapshot_is_json(self):
        game = spire.start_combat(1, {"encounter": "Cultist"})
        state = game.snapshot()
        self.assertEqual(state, json.loads(json.dumps(state)))
        self.assertEqual(20, state["run"]["ascension"])
        self.assertEqual(7, len(state["combat"]["hand"]))
        self.assertEqual([{"id": "Ring of the Snake", "counter": -1}], state["run"]["relics"])

    def test_scenario_replaces_deck_and_relics(self):
        game = spire.start_combat(2, {
            "encounter": "Jaw Worm", "deck": [{"id": "Defend_G", "upgrades": 1}],
            "relics": [], "potions": [None, None], "hp": 12, "max_hp": 30, "gold": 4,
        })
        state = game.snapshot()
        self.assertEqual(12, state["run"]["hp"])
        self.assertEqual(30, state["run"]["max_hp"])
        self.assertEqual(4, state["run"]["gold"])
        self.assertEqual([], state["run"]["relics"])
        self.assertEqual([{"id": "Defend_G", "upgrades": 1, "misc": 0}], state["run"]["master_deck"])
        self.assertEqual("Defend_G", state["combat"]["hand"][0]["id"])

    def test_empty_deck_is_empty(self):
        game = spire.start_combat(3, {"encounter": "Cultist", "deck": [], "relics": []})
        self.assertEqual([], game.snapshot()["combat"]["hand"])
        self.assertEqual([{"type": "end_turn"}], game.legal_actions())

    def test_signed_java_seed(self):
        game = spire.start_combat(-1, {"encounter": "Cultist"})
        self.assertEqual(-1, game.snapshot()["run"]["seed"])

    def test_batch_matches_independent_games(self):
        seeds = [11, 31, 17]
        scenario = {"encounter": "2 Louse"}
        games = [spire.start_combat(seed, scenario) for seed in seeds]
        batch = spire.Batch(len(games))
        batch.start_combat(seeds, scenario)
        self.assertEqual([game.snapshot() for game in games], batch.snapshot())
        for _ in range(3):
            actions = [game.legal_actions()[0] for game in games]
            for game, action in zip(games, actions):
                game.step(action)
            batch.step(actions)
            self.assertEqual([game.snapshot() for game in games], batch.snapshot())

    def test_terminal_combat_and_replayer_wiring(self):
        scenario = {"encounter": "Cultist", "deck": [], "relics": [], "hp": 1}
        game = spire.start_combat(8, scenario)
        records = [{"seed": 8, "character": "THE_SILENT", "ascension": 20, "scenario": scenario}]
        decisions = 0
        while game.legal_actions():
            actions = game.legal_actions()
            action = actions[0]
            records.append({"snapshot": game.snapshot(), "legal": actions, "action": action})
            game.step(action)
            decisions += 1
            self.assertLess(decisions, 10)
        records.append({"snapshot": game.snapshot()})
        self.assertEqual("death", game.snapshot()["screen"]["type"])
        with tempfile.TemporaryDirectory() as temporary:
            trace = Path(temporary) / "self_trace.jsonl"
            trace.write_text("".join(json.dumps(record) + "\n" for record in records))
            self.assertEqual(decisions, replay(trace, spire))

    def test_all_encounters_reach_terminal(self):
        policy = random.Random(12)
        for seed, encounter in enumerate(spire.encounters):
            with self.subTest(encounter=encounter):
                game = spire.start_combat(seed, {"encounter": encounter})
                decisions = 0
                while game.legal_actions():
                    game.step(policy.choice(game.legal_actions()))
                    decisions += 1
                    self.assertLess(decisions, 2000)
                self.assertIn(game.snapshot()["screen"]["type"], ["victory", "death"])


if __name__ == "__main__":
    unittest.main()
