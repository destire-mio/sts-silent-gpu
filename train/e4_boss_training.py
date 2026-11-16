"""Opt-in Stage12 practice with naturally reached Boss starts."""
from collections import Counter

import torch

from train.e4 import ACT, BOSS_ROOM, COMBAT_PHASE, DEATH, FLOOR, HP, PHASE, ROOM, SCREEN, VICTORY


PRACTICE = "arena_boss_reset"
NODES = ("act1_boss", "act2_boss", "act3_first_boss", "act3_second_boss")
PRACTICE_HP_WEIGHTS = (.25, .25, 1., .25)


def practice_reward(won, died, entry_hp, final_hp, max_hp, weight):
    lost = 1. if died else max(0, entry_hp - final_hp) / max_hp
    return float(won) - weight * lost, lost


def boss_identity(progress):
    act = progress[ACT]
    if act <= 3 and progress[ROOM] == BOSS_ROOM and progress[PHASE] == COMBAT_PHASE:
        return act, progress[FLOOR]
    return None


class BossTraining:
    def __init__(self, collector, fraction, capacity, offset, *, minimum=256, reward_mode="current",
                 hp_weights=PRACTICE_HP_WEIGHTS):
        self.collector = collector
        self.fraction = fraction
        self.capacity = capacity
        self.base = collector.search.lanes + offset
        self.minimum = minimum
        self.entries = []
        self.position = 0
        self.active = {}
        self.counts = {node: Counter() for node in NODES}
        self.reward_mode = reward_mode
        self.hp_weights = hp_weights
        self.hp_anchors = {}
        if reward_mode == "hp":
            collector.search.boss_practice_values = self.hp_anchors

    def practice_nodes(self, slots):
        return [NODES.index(self.active[s][1]) if s in self.hp_anchors else -1 for s in slots]

    def restart(self, slots):
        if not self.fraction or len(self.entries) < self.minimum:
            return slots
        collector = self.collector
        choices = torch.rand(len(slots), generator=collector.seed_rng).tolist()
        ordinary, practice = [], []
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
            practice.append(slot)
        if self.reward_mode == "hp" and practice:
            from train.e4_stage12_auxiliary import player_max_hp
            progress = collector.search.progress_for(practice)
            for slot, max_hp in zip(practice, player_max_hp(collector.search, practice).tolist()):
                assert 0 < progress[slot][HP] <= max_hp
                self.hp_anchors[slot] = (progress[slot][HP], max_hp)
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

    def after_step(self, model, slots, before, terminal, bootstrap, completed_before, auxiliary=None, rewards=None):
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
            if slot in self.hp_anchors:
                rewards[i] = 0.
            if not terminal[i] and identity == target:
                continue
            del self.active[slot]
            anchor = self.hp_anchors.pop(slot, None)
            terminal[i] = True
            collector.seeds[slot] = None
            if slot in collector.stalled:
                self.counts[node]["faults"] += 1
                continue
            won = target in collector.rewards[slot].bosses
            self.counts[node]["completed"] += 1
            self.counts[node]["wins"] += int(won)
            if anchor is not None:
                entry_hp, max_hp = anchor
                died = progress[SCREEN] == DEATH or progress[HP] <= 0
                rewards[i], lost = practice_reward(won, died, entry_hp, progress[HP], max_hp,
                                                   self.hp_weights[NODES.index(node)])
                self.counts[node]["hp_completed"] += 1
                self.counts[node]["hp_lost"] += lost
            elif won and progress[SCREEN] not in (DEATH, VICTORY):
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

    def hp_metrics(self, fragment):
        moments = torch.zeros((len(NODES), 7), dtype=torch.float64, device=self.collector.search.device)
        if fragment is not None and "practice_node" in fragment:
            nodes = fragment["practice_node"]
            valid = (nodes >= 0) & fragment["action_valid"]
            target = fragment["returns"][valid].double()
            error = target - fragment["values"][valid].double()
            moments[:, :5].index_add_(0, nodes[valid].long(), torch.stack(
                (torch.ones_like(target), target, target.square(), error, error.square()), dim=1))
        moments[:, 5:] = moments.new_tensor(
            [[self.counts[node]["hp_completed"], self.counts[node]["hp_lost"]] for node in NODES])
        if torch.distributed.is_initialized() and torch.distributed.get_world_size() > 1:
            torch.distributed.all_reduce(moments)
        result = {}
        for node, (count, total, square, errors, error_square, completed, lost) in zip(NODES, moments.tolist()):
            variance = square / count - (total / count)**2 if count else 0.
            error_variance = max(0., error_square / count - (errors / count)**2) if count else 0.
            result[node] = dict(hp_completed=int(completed), mean_hp_loss_fraction=lost / completed if completed else None,
                                practice_value_samples=int(count),
                                practice_value_explained_variance=1. - error_variance / variance if variance > 0. else None)
        return result

    def take_metrics(self, fragment=None):
        hp = self.hp_metrics(fragment) if self.reward_mode == "hp" else {}
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
            row.update(hp.get(node, {}))
            rows[node] = row
            self.counts[node].clear()
        pool_size, capacity, _, _ = totals[-1]
        metrics = dict(fraction=self.fraction, pool_size=pool_size, pool_capacity=capacity, by_node=rows)
        if self.reward_mode == "hp":
            metrics.update(reward_mode=self.reward_mode, hp_weights=self.hp_weights)
        return metrics

    def state(self):
        state = dict(entries=self.entries, position=self.position, active=self.active,
                     counts={node: dict(counts) for node, counts in self.counts.items()})
        if self.reward_mode == "hp":
            state["hp_anchors"] = self.hp_anchors
        return state

    def restore(self, state):
        self.entries = state["entries"]
        self.position = state["position"]
        self.active = state["active"]
        if self.reward_mode == "hp":
            self.hp_anchors.update(state.get("hp_anchors", {}))
        self.counts = {node: Counter(counts) for node, counts in state["counts"].items()}
        slots = list(range(self.base, self.base + len(self.entries)))
        for slot, entry in zip(slots, self.entries):
            self.collector.restore_pool_metadata(slot, entry)
        self.collector.replay(slots, self.entries)
