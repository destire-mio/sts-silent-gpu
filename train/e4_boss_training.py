"""Opt-in Stage12 practice with naturally reached Boss starts."""
from collections import Counter

import torch

from train.e4 import ACT, BOSS_ROOM, COMBAT_PHASE, DEATH, FLOOR, PHASE, ROOM, SCREEN, VICTORY


PRACTICE = "arena_boss_reset"
NODES = ("act1_boss", "act2_boss", "act3_first_boss", "act3_second_boss")


def boss_identity(progress):
    act = progress[ACT]
    if act <= 3 and progress[ROOM] == BOSS_ROOM and progress[PHASE] == COMBAT_PHASE:
        return act, progress[FLOOR]
    return None


class BossTraining:
    def __init__(self, collector, fraction, capacity, offset, *, minimum=256):
        self.collector = collector
        self.fraction = fraction
        self.capacity = capacity
        self.base = collector.search.lanes + offset
        self.minimum = minimum
        self.entries = []
        self.position = 0
        self.active = {}
        self.counts = {node: Counter() for node in NODES}

    def restart(self, slots):
        if not self.fraction or len(self.entries) < self.minimum:
            return slots
        collector = self.collector
        choices = torch.rand(len(slots), generator=collector.seed_rng).tolist()
        ordinary = []
        for slot, choice in zip(slots, choices):
            if choice >= self.fraction:
                ordinary.append(slot)
                continue
            i = int(torch.randint(len(self.entries), (), generator=collector.seed_rng))
            entry = self.entries[i]
            collector.start_from_entry(entry, self.base + i, slot, PRACTICE)
            collector.captured[slot] = [True] * len(collector.captured[slot])
            self.active[slot] = (entry["boss_identity"], entry["node"])
            self.counts[entry["node"]]["starts"] += 1
        return ordinary

    def capture(self, roots):
        if not roots:
            return
        collector = self.collector
        if len(roots) >= self.capacity:
            self.entries.clear()
            self.position = 0
            roots = roots[-self.capacity:]
        indices = [(self.position + i) % self.capacity for i in range(len(roots))]
        collector.search.clone([slot for slot, _ in roots], [self.base + i for i in indices])
        for i, (slot, identity) in zip(indices, roots):
            act, _ = identity
            node = NODES[act - 1] if act < 3 else NODES[2 + int(collector.rewards[slot].reach[NODES[2]])]
            entry = collector.pool_entry(slot, self.base + i)
            entry.update(boss_identity=identity, node=node)
            if i == len(self.entries):
                self.entries.append(entry)
            else:
                self.entries[i] = entry
        self.position = (self.position + len(roots)) % self.capacity

    def after_step(self, model, slots, before, terminal, bootstrap, completed_before, auxiliary=None):
        collector = self.collector
        after = collector.search.last_step_progress
        roots, wins = [], []
        for i, slot in enumerate(slots):
            progress = after.get(slot, before[slot])
            identity = boss_identity(progress)
            if slot not in self.active:
                source = collector.starts[slot]
                if (self.fraction and not terminal[i] and identity is not None
                        and identity != boss_identity(before[slot])
                        and not (source.startswith("late_") and source.endswith("_e2"))):
                    roots.append((slot, identity))
                continue
            target, node = self.active[slot]
            if not terminal[i] and identity == target:
                continue
            del self.active[slot]
            terminal[i] = True
            collector.seeds[slot] = None
            if slot in collector.stalled:
                self.counts[node]["faults"] += 1
                continue
            won = target in collector.rewards[slot].bosses
            self.counts[node]["completed"] += 1
            self.counts[node]["wins"] += int(won)
            if won and progress[SCREEN] not in (DEATH, VICTORY):
                wins.append((i, slot))
        self.capture(roots)
        collector.completed[completed_before:] = [episode for episode in collector.completed[completed_before:]
                                                if episode["start_kind"] != PRACTICE]
        if not wins:
            return {}
        winning_slots = [slot for _, slot in wins]
        predictions = {}
        if auxiliary and "global" in auxiliary:
            from train.e4_stage12_auxiliary import Capture
            capture = Capture(model)
            scores = collector.search.scores(capture, winning_slots, after)
            predictions = dict(zip(winning_slots, torch.cat(capture.outputs["global"]).sigmoid()))
        else:
            scores = collector.search.scores(model, winning_slots, after)
        for i, slot in wins:
            bootstrap[i] = scores[slot]
        return predictions

    def take_metrics(self):
        fields = ("starts", "completed", "wins", "faults")
        totals = [[self.counts[node][field] for field in fields] for node in NODES]
        totals.append([len(self.entries), self.capacity, 0, 0])
        if torch.distributed.is_initialized() and torch.distributed.get_world_size() > 1:
            tensor = torch.tensor(totals, dtype=torch.int64, device=self.collector.search.device)
            torch.distributed.all_reduce(tensor)
            totals = tensor.tolist()
        rows = {}
        for node, values in zip(NODES, totals):
            row = dict(zip(fields, values))
            row["win_rate"] = row["wins"] / row["completed"] if row["completed"] else None
            rows[node] = row
            self.counts[node].clear()
        pool_size, capacity, _, _ = totals[-1]
        return dict(fraction=self.fraction, pool_size=pool_size, pool_capacity=capacity, by_node=rows)

    def state(self):
        return dict(entries=self.entries, position=self.position, active=self.active,
                    counts={node: dict(counts) for node, counts in self.counts.items()})

    def restore(self, state):
        self.entries = state["entries"]
        self.position = state["position"]
        self.active = state["active"]
        self.counts = {node: Counter(counts) for node, counts in state["counts"].items()}
        slots = list(range(self.base, self.base + len(self.entries)))
        for slot, entry in zip(slots, self.entries):
            self.collector.restore_pool_metadata(slot, entry)
        self.collector.replay(slots, self.entries)
