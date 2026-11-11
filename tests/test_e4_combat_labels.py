from types import SimpleNamespace

from torch import nn

from tools.e4_combat_labels import capture_combat_episodes
from train.e4 import MAP, SCREEN


def test_capture_does_not_reset_when_source_shard_has_no_fresh_seeds():
    class FakeRunner:
        def __init__(self, source_shard):
            self.seeds = [None] * source_shard
            self.completed = []
            self.resets = []
            self.steps = 0
            self.search = SimpleNamespace(clock=lambda: None, failed=set(),
                progress_for=lambda slots: {slot: {SCREEN: MAP} for slot in slots})

        def reset(self, slots, seeds):
            assert len(slots) == len(seeds) > 0, "CUDA reset requires at least one slot"
            self.resets.append((slots.copy(), seeds.copy()))
            for slot, seed in zip(slots, seeds):
                self.seeds[slot] = seed

        def choose(self, model, slots, before):
            return [0], None, None, None

        def advance(self, slots, actions, before):
            self.steps += 1
            done = self.steps == 2
            if done:
                slot = slots[0]
                self.completed.append(dict(seed=self.seeds[slot], candidates=[0, 0], outcome="finished"))
                self.seeds[slot] = None
            return None, [done]

    runner = FakeRunner(source_shard=2)
    seeds = [1_000_000_000]
    episodes = capture_combat_episodes(nn.Identity(), runner, seeds)

    assert runner.resets == [([0], seeds)]
    assert runner.steps == 2
    assert episodes == [dict(seed=seeds[0], path=[0, 0], outcome="finished", roots=[])]
