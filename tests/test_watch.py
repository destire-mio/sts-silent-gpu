import copy
import unittest

import numpy as np
import spire
from watch.run import Difference, Lockstep


def request(game, decision=0, previous=None):
    return dict(seed=1, decision=decision, previous=previous, snapshot=game.snapshot(),
                legal=game.legal_actions(), terminal=None)


class WatchTest(unittest.TestCase):
    def setUp(self):
        self.calls = []

        def select(observation):
            self.calls.append(observation)
            return 0

        self.lockstep = Lockstep(1, [], select)
        self.original = spire.reset(1)

    def test_acknowledged_action_advances_once(self):
        response = self.lockstep.decision(request(self.original))
        self.assertEqual(response, dict(index=0, policy=True))
        self.assertEqual(len(self.calls[0]), 8)
        previous = copy.deepcopy(self.lockstep.pending)
        self.assertEqual(self.lockstep.game.snapshot(), self.original.snapshot())
        self.original.step(previous["action"])
        self.lockstep.decision(request(self.original, 1, previous))
        self.assertEqual(self.lockstep.game.snapshot(), self.original.snapshot())
        self.assertEqual(len(self.calls), 2)

    def test_divergence_prevents_policy_and_action(self):
        for field in ("hp", "legal", "seed", "decision", "previous", "terminal"):
            with self.subTest(field=field):
                message = request(self.original)
                if field == "hp":
                    player = message["snapshot"]["player"]["ref"]
                    message["snapshot"]["objects"][player]["fields"]["AbstractCreature.currentHealth"] -= 1
                elif field == "legal":
                    message[field] = []
                elif field == "previous":
                    message[field] = dict(index=0, action=dict(type="end_turn"))
                elif field == "terminal":
                    message[field] = "victory"
                else:
                    message[field] += 1
                with self.assertRaises(Difference):
                    self.lockstep.decision(message)
                self.assertEqual(self.calls, [])
                self.assertIsNone(self.lockstep.pending)
                self.assertEqual(self.lockstep.game.snapshot(), self.original.snapshot())

    def test_prefix_does_not_invoke_policy(self):
        self.lockstep.prefix = [0]
        response = self.lockstep.decision(request(self.original))
        self.assertEqual(response, dict(index=0, policy=False))
        self.assertEqual(self.calls, [])

    def test_prefix_preserves_displayed_rest_heal_after_fruit_juice(self):
        scenario = dict(encounter="Cultist", deck=[], relics=["Ring of the Snake"],
                        potions=["Fruit Juice", None], hp=50, max_hp=66,
                        start=dict(act=1, floor=1, node=[0, 0], room="RestRoom", gold=99,
                                   keys=[False, False, False], emerald=False))
        self.original = spire.start_combat(1, scenario)
        self.lockstep.game = spire.start_combat(1, scenario)
        self.original.observation()
        index = self.original.legal_actions().index(dict(type="potion_use", slot=0, target=-1))
        self.lockstep.prefix = [index]
        self.lockstep.decision(request(self.original))
        previous = copy.deepcopy(self.lockstep.pending)
        self.original.step(previous["action"])
        expected = self.original.observation()
        self.lockstep.decision(request(self.original, 1, previous))
        for wanted, actual in zip(expected, self.calls[0]):
            np.testing.assert_array_equal(wanted, actual)


if __name__ == "__main__":
    unittest.main()
