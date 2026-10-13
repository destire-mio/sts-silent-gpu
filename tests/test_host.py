import json
import random
import tempfile
import unittest
from pathlib import Path

import spire
from replay import GAME_SHA256, replay


def fields(state, reference):
    return state["objects"][reference["ref"]]["fields"]


def player(state):
    return fields(state, state["player"])


def pile(state, name):
    group = fields(state, player(state)["AbstractPlayer." + name])
    return [fields(state, card) for card in group["CardGroup.group"]]


class HostInterface(unittest.TestCase):
    def test_fault_api_and_indexed_reset(self):
        batch = spire.Batch(2)
        batch.start_combat([1, 2], {"encounter": "Cultist"})
        batch.step([{"type": "end_turn"}, {"type": "end_turn"}])
        other = batch.snapshot()[1]
        batch.reset_at(0, 19)
        self.assertEqual([0, 0], batch.faults())
        self.assertEqual(19, batch.snapshot()[0]["run"]["seed"])
        self.assertEqual(other, batch.snapshot()[1])
        self.assertEqual(0, spire.reset(1).fault)

    def test_snapshot_is_json(self):
        game = spire.start_combat(1, {"encounter": "Cultist"})
        state = game.snapshot()
        self.assertEqual(state, json.loads(json.dumps(state)))
        self.assertEqual(20, state["run"]["ascensionLevel"])
        self.assertEqual(7, len(pile(state, "hand")))
        relic = fields(state, player(state)["AbstractPlayer.relics"][0])
        self.assertEqual("Ring of the Snake", relic["AbstractRelic.relicId"])
        self.assertEqual(-1, relic["AbstractRelic.counter"])

    def test_scenario_replaces_deck_and_relics(self):
        game = spire.start_combat(2, {
            "encounter": "Jaw Worm", "deck": [{"id": "Defend_G", "upgrades": 1}],
            "relics": [], "potions": [None, None], "hp": 12, "max_hp": 30, "gold": 4,
        })
        state = game.snapshot()
        self.assertEqual(12, player(state)["AbstractCreature.currentHealth"])
        self.assertEqual(30, player(state)["AbstractCreature.maxHealth"])
        self.assertEqual(4, player(state)["AbstractCreature.gold"])
        self.assertEqual([], player(state)["AbstractPlayer.relics"])
        deck = pile(state, "masterDeck")
        self.assertEqual(1, len(deck))
        self.assertEqual("Defend_G", deck[0]["AbstractCard.cardID"])
        self.assertEqual(1, deck[0]["AbstractCard.timesUpgraded"])
        self.assertEqual(deck[0]["AbstractCard.uuid"], pile(state, "hand")[0]["AbstractCard.uuid"])
        self.assertEqual("Defend_G", pile(state, "hand")[0]["AbstractCard.cardID"])

    def test_empty_deck_is_empty(self):
        game = spire.start_combat(3, {"encounter": "Cultist", "deck": [], "relics": []})
        self.assertEqual([], pile(game.snapshot(), "hand"))
        self.assertEqual([{"type": "end_turn"}], game.legal_actions())

    def test_hand_selection_preserves_card_and_group_identity(self):
        game = spire.start_combat(11, {
            "encounter": "Cultist", "relics": [],
            "deck": [{"id": "Survivor", "upgrades": 0}] * 4,
        })
        game.step({"type": "play", "card": 0, "target": -1})
        state = game.snapshot()
        self.assertEqual("HAND_SELECT", state["screen"]["type"])
        self.assertEqual(["Survivor"] * 3, state["screen"]["options"])
        selection = fields(state, state["screen"]["state"])
        self.assertEqual(player(state)["AbstractPlayer.hand"], selection["HandCardSelectScreen.hand"])
        self.assertFalse(selection["HandCardSelectScreen.wereCardsRetrieved"])
        chosen = fields(state, selection["HandCardSelectScreen.hand"])["CardGroup.group"][0]
        chosen_uuid = fields(state, chosen)["AbstractCard.uuid"]
        master_index = [card["AbstractCard.uuid"] for card in pile(state, "masterDeck")].index(chosen_uuid)
        game.step({"type": "choose", "option": 0})
        state = game.snapshot()
        self.assertEqual([], state["screen"]["options"])
        selection = fields(state, state["screen"]["state"])
        selected = fields(state, selection["HandCardSelectScreen.selectedCards"])["CardGroup.group"]
        self.assertEqual([pile(state, "masterDeck")[master_index]["AbstractCard.uuid"]],
                         [fields(state, card)["AbstractCard.uuid"] for card in selected])
        self.assertEqual([{"type": "proceed"}], game.legal_actions())
        game.step({"type": "proceed"})
        self.assertEqual("NONE", game.snapshot()["screen"]["type"])

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
        records = [{"schema": 2, "game_sha256": GAME_SHA256, "seed": 8, "character": "THE_SILENT", "ascension": 20, "scenario": scenario}]
        decisions = 0
        while game.legal_actions():
            actions = game.legal_actions()
            action = actions[0]
            records.append({"snapshot": game.snapshot(), "legal": actions, "action": action})
            game.step(action)
            decisions += 1
            self.assertLess(decisions, 10)
        records.append({"snapshot": game.snapshot(), "terminal": "death"})
        self.assertEqual("DEATH", game.snapshot()["screen"]["type"])
        self.assertEqual("DEATH", game.snapshot()["screen"]["choice"])
        with tempfile.TemporaryDirectory() as temporary:
            trace = Path(temporary) / "self_trace.jsonl"
            trace.write_text("".join(json.dumps(record) + "\n" for record in records))
            result = replay(trace, spire)
            self.assertTrue(result.passed)
            self.assertEqual(decisions, result.decisions)
            self.assertEqual(decisions, result.scopes["combat"].matched_decisions)

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
                state = game.snapshot()
                room = fields(state, state["room"])
                self.assertTrue(player(state)["AbstractCreature.isDead"] or room["AbstractRoom.isBattleOver"])


if __name__ == "__main__":
    unittest.main()
