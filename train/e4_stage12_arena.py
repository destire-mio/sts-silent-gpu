"""Standalone practice segments drawn from naturally encountered hard fights."""
from collections import Counter

import torch

from train.e4 import ACT, BATTLE_OVER, BOSS_ROOM, COMBAT_PHASE, DEATH, ELITE_ROOM, FLOOR, HP, PHASE, ROOM, SCREEN, SMOKED, VICTORY
from train.e4_midstart import MidstartCollector

CATEGORIES = {
    (2, ELITE_ROOM): "act2_elite", (2, BOSS_ROOM): "act2_boss",
    (3, ELITE_ROOM): "act3_elite", (3, BOSS_ROOM): "act3_boss",
    (4, ELITE_ROOM): "act4_elite", (4, BOSS_ROOM): "heart",
}
POOL_CAPACITY = 512
POOL_SLOTS = len(CATEGORIES) * POOL_CAPACITY
BOSS_WEIGHTS = {"act2_boss": 2, "act3_boss": 1, "heart": 1}


def battle_identity(progress):
    return (progress[ACT], progress[FLOOR], progress[ROOM]) if progress[PHASE] == COMBAT_PHASE else None


class ArenaCollector(MidstartCollector):
    def __init__(self, search, seed_rng, *, rank, fraction):
        super().__init__(search, seed_rng, rank=rank)
        self.arena_lanes = max(1, round(search.lanes * fraction))
        self.arena_pools = {category: [] for category in CATEGORIES.values()}
        self.arena_positions = dict.fromkeys(self.arena_pools, 0)
        self.arena_bases = {category: search.lanes + 1024 + i * POOL_CAPACITY
                            for i, category in enumerate(self.arena_pools)}
        self.arena_entries = {}
        self.arena_fights = {}
        self.arena_stats = {category: Counter() for category in self.arena_pools}
        self.arena_end_targets = {}

    def capture_starts(self, slots, progress):
        ordinary = [slot for slot in slots if slot not in self.arena_fights]
        super().capture_starts(ordinary, progress)
        for slot in slots:
            category = self.arena_entries.pop(slot, None)
            if category is None or slot in self.search.failed:
                continue
            position = self.arena_positions[category]
            target = self.arena_bases[category] + position
            self.search.clone([slot], [target])
            entries = self.arena_pools[category]
            entry = self.pool_entry(slot, target)
            if position == len(entries):
                entries.append(entry)
            else:
                entries[position] = entry
            self.arena_positions[category] = (position + 1) % POOL_CAPACITY

    def choose_category(self):
        bosses = [category for category, weight in BOSS_WEIGHTS.items()
                  if self.arena_pools[category] for _ in range(weight)]
        elites = [category for category, pool in self.arena_pools.items()
                  if pool and category not in BOSS_WEIGHTS]
        if bosses and elites:
            group = bosses if int(torch.randint(4, (), generator=self.seed_rng)) < 3 else elites
        else:
            group = bosses or elites
        return group[int(torch.randint(len(group), (), generator=self.seed_rng))]

    def fresh(self, slots):
        if not any(self.arena_pools.values()):
            return super().fresh(slots)
        practice = [slot for slot in slots if slot < self.arena_lanes]
        ordinary = [slot for slot in slots if slot >= self.arena_lanes]
        if ordinary:
            super().fresh(ordinary)
        for slot in practice:
            category = self.choose_category()
            entries = self.arena_pools[category]
            position = int(torch.randint(len(entries), (), generator=self.seed_rng))
            self.start_from_entry(entries[position], self.arena_bases[category] + position, slot, "arena_" + category)
            self.arena_fights[slot] = category
            self.arena_stats[category]["started"] += 1

    def after_transition(self, model, slots, progress, rewards, terminal, auxiliary):
        """End the arena segment with a detached value at a successful combat exit."""
        after = self.search.last_step_progress
        terminal_indices = [i for i, ended in enumerate(terminal) if ended]
        if terminal_indices:
            tail = self.completed[-len(terminal_indices):]
            self.completed[-len(terminal_indices):] = [
                episode for i, episode in zip(terminal_indices, tail) if slots[i] not in self.arena_fights]
        wins = []
        open_ends = []
        self.arena_end_targets = {}
        for i, slot in enumerate(slots):
            category = self.arena_fights.get(slot)
            if category is None:
                if self.starts[slot] == "fresh" and slot not in self.stalled and not terminal[i]:
                    next_progress = after[slot]
                    if battle_identity(next_progress) is not None and battle_identity(progress[slot]) != battle_identity(next_progress):
                        next_category = CATEGORIES.get((next_progress[ACT], next_progress[ROOM]))
                        if next_category:
                            self.arena_entries[slot] = next_category
                continue
            if not terminal[i] and battle_identity(after[slot]) == battle_identity(progress[slot]):
                continue
            del self.arena_fights[slot]
            terminal[i] = True
            self.seeds[slot] = None
            if slot in self.stalled:
                self.arena_stats[category]["faults"] += 1
                continue
            p = after[slot]
            won = p[BATTLE_OVER] and not p[SMOKED] and p[HP] > 0 and p[SCREEN] != DEATH
            self.arena_stats[category]["games"] += 1
            self.arena_stats[category]["wins"] += int(won)
            if p[SCREEN] not in (DEATH, VICTORY):
                open_ends.append(slot)
                if won:
                    wins.append((i, slot))
            if not won:
                rewards[i] = 0.
        if open_ends and auxiliary and "global" in auxiliary:
            from train.e4_stage12_auxiliary import Capture
            capture = Capture(model)
            scores = self.search.scores(capture, open_ends, after)
            probabilities = torch.cat(capture.outputs["global"]).sigmoid()
            self.arena_end_targets = dict(zip(open_ends, probabilities))
        elif wins:
            scores = self.search.scores(model, [slot for _, slot in wins], after)
        for i, slot in wins:
            rewards[i] += scores[slot]

    def take_arena_metrics(self):
        result = {}
        for category, counts in self.arena_stats.items():
            games, wins = counts["games"], counts["wins"]
            result[category] = dict(games=games, wins=wins, win_rate=wins / games if games else None,
                                    starts=counts["started"], faults=counts["faults"],
                                    pool_size=len(self.arena_pools[category]))
            counts.clear()
        return result

    def state(self):
        state = super().state()
        state["arena"] = dict(pools=self.arena_pools, positions=self.arena_positions,
                              entries=self.arena_entries, fights=self.arena_fights, stats=self.arena_stats)
        return state

    def restore(self, state):
        super().restore(state)
        if "arena" not in state:
            return
        saved = state["arena"]
        self.arena_pools = saved["pools"]
        self.arena_positions = saved["positions"]
        self.arena_entries = saved["entries"]
        self.arena_fights = saved["fights"]
        self.arena_stats = {category: Counter(counts) for category, counts in saved["stats"].items()}
        slots, entries = [], []
        for category, pool in self.arena_pools.items():
            for position, entry in enumerate(pool):
                slot = self.arena_bases[category] + position
                self.restore_pool_metadata(slot, entry)
                slots.append(slot)
                entries.append(entry)
        self.replay(slots, entries)
