from collections import deque
import math

import torch

from train.e4 import RoomSearch, SegmentCollector, AllocationEnded, COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY, ACT, FLOOR, BATTLE_OVER, SMOKED, HP
from train.e4_stage6.copy_tape import CombatTape


class CopySearch(RoomSearch):
    def __init__(self, lanes, device, rng, selection_rng, *, bosses_seen=511):
        super().__init__(lanes, 32, device, rng, scratch_slots=lanes * 33, bosses_seen=bosses_seen)
        self.tape = CombatTape(selection_rng)

    def predict(self, model, slots):
        combat_values = []
        def run_policy(inputs):
            logits, value, combat_value = model(inputs)
            combat_values.extend(combat_value.detach().cpu().tolist())
            return logits, value
        actions, values, logs = super().predict(run_policy, slots)
        if self.contexts and self.contexts[-1]['kind'] == 'combat':
            self.tape.record(self, self.contexts[-1], slots, actions, logs, combat_values)
        return actions, values, logs

    def scores(self, model, slots, progress):
        return super().scores(model.run_policy, slots, progress)

    def plan(self, model, sources, kind):
        count = len(self.tape.groups)
        rng_before = self.tape.rng.get_state().clone()
        try:
            return super().plan(model, sources, kind)
        except AllocationEnded:
            del self.tape.groups[count:]
            self.tape.rng.set_state(rng_before)
            raise

    def combat(self, model, sources):
        selected_paths = {}
        capacity = len(self.available) // self.trials
        assert capacity > 0
        for begin in range(0, len(sources), capacity):
            parents = sources[begin:begin + capacity]
            slots = self.take(len(parents) * self.trials)
            context = self.begin_search("combat", slots)
            context["copy_rows"] = {s: [] for s in slots}
            if self.combat_diagnostics is not None:
                starts = self.progress()
                context["starts"] = {p: (starts[p][ACT], starts[p][FLOOR], self.provenance[p][0]) for p in parents}
                context["entropy_sum"] = dict.fromkeys(slots, 0.)
                context["entropy_count"] = dict.fromkeys(slots, 0)
            try:
                self.clone([s for s in parents for _ in range(self.trials)], slots)
                self.capacity_metrics["combat_trials"] += len(slots)
                paths = {s: [] for s in slots}
                capped = set()
                live = slots[:]
                while live:
                    context["live"] = live
                    self.clock()
                    choices, _, _ = self.predict(model, live)
                    self.trace_step(context, live, choices)
                    capped.update(self.step(live, choices, context))
                    self.searched_decisions += len(live)
                    for s, a in zip(live, choices):
                        paths[s].append(a)
                    progress = self.progress()
                    continuing = [s for s in live if s not in capped and progress[s][PHASE] == COMBAT_PHASE
                                  and progress[s][SCREEN] not in (DEATH, VICTORY)]
                    finished = set(live) - set(continuing) - capped
                    for s in finished:
                        self.metrics["combat_normal_max_steps"] = max(self.metrics["combat_normal_max_steps"], len(paths[s]))
                    expired = [s for s in continuing if len(paths[s]) > 3000]
                    self.exceeded(context, expired)
                    capped.update(expired)
                    live = [s for s in continuing if s not in capped]
                eligible = [s for s in slots if s not in capped]
                assert not capped, "Cannot learn a centered32 group with unfinished/capped trials"
                scores = self.scores(model, eligible, progress)
                if self.combat_diagnostics is not None:
                    keys = dict(zip(eligible, self.loop_keys(eligible))) if eligible else {}
                    for j, parent in enumerate(parents):
                        trials = slots[j * self.trials:(j + 1) * self.trials]
                        group = [s for s in trials if s not in capped]
                        act, floor, seed = context["starts"][parent]
                        count = sum(context["entropy_count"][s] for s in trials)
                        entropy_sum = sum(context["entropy_sum"][s] for s in trials)
                        row = dict(act=act, floor=floor, seed=seed, trials=len(trials),
                            capped_trials=len(trials)-len(group), decision_count=count,
                            entropy_sum=entropy_sum, policy_entropy=entropy_sum/count,
                            nested_event=any(c["kind"] == "event" for c in self.contexts),
                            win_fraction=None, score_std=None, distinct_end_states=None, selected_score_gain=None)
                        if len(group) == len(trials):
                            mean = sum(scores[s] for s in group)/len(group)
                            row.update(win_fraction=sum(progress[s][BATTLE_OVER] and not progress[s][SMOKED]
                                and progress[s][HP] > 0 for s in group)/len(group),
                                score_std=math.sqrt(sum((scores[s]-mean)**2 for s in group)/len(group)),
                                distinct_end_states=len({keys[s] for s in group}),
                                selected_score_gain=max(scores[s] for s in group)-mean)
                        self.combat_diagnostics.append(row)
                for j, parent in enumerate(parents):
                    group = [s for s in slots[j * self.trials:(j + 1) * self.trials] if s not in capped]
                    if group:
                        winner = max(group, key=scores.get)
                        self.tape.finish(self, context, group, {s: scores[s] for s in group}, winner, parent)
                        self.clone([winner], [parent])
                        selected_paths[parent] = paths[winner]
                        self.combat_tails[parent] = list(context["tail"][winner])
                    else:
                        selected_paths[parent] = None
            finally:
                self.contexts.pop()
                self.available.extend(slots)
        return selected_paths



class TrainingCollector(SegmentCollector):
    def __init__(self, search, seed_rng, *, rank=0):
        super().__init__(search, seed_rng, rank=rank)
        self.training_seed_rows = []

    def reset(self, slots, seeds):
        assert all(1000000000 <= seed < 2000000000 for seed in seeds), 'Dev seed cannot enter training collector'
        return super().reset(slots, seeds)

    def choose(self, model, slots, progress):
        assert self.evaluation_waiting is None, 'Evaluation requires a separate collector'
        seeds = [self.seeds[s] for s in slots]
        assert all(1000000000 <= seed < 2000000000 for seed in seeds), 'Dev seed cannot enter training fragment'
        assert all(self.search.provenance[s][0] == self.seeds[s] for s in slots)
        rng_before = self.search.rng.get_state().clone()
        available_before = self.search.available.copy()
        pending_before = [list(pending) for pending in self.pending]
        try:
            result = super().choose(model, slots, progress)
        except AllocationEnded:
            assert not self.search.contexts
            self.search.rng.set_state(rng_before)
            self.search.available = available_before
            self.pending = [deque(pending) for pending in pending_before]
            raise
        self.training_seed_rows.extend(seeds)
        return result

    def state(self):
        state = super().state()
        state['training_seed_rows'] = self.training_seed_rows.copy()
        return state

    def restore(self, state):
        assert state['evaluation_waiting'] is None
        assert all(seed is None or 1000000000 <= seed < 2000000000 for seed in state['seeds'])
        assert all(1000000000 <= seed < 2000000000 for seed in state['training_seed_rows'])
        super().restore(state)
        self.training_seed_rows = state['training_seed_rows'].copy()

    def collect(self, model, length=64):
        result = super().collect(model, length)
        result['training_seeds'] = torch.tensor(self.training_seed_rows, dtype=torch.int64, device=self.search.device)
        result['source_partition'] = 'train'
        self.training_seed_rows = []
        return result


class CopyCollector(TrainingCollector):
    def choose(self, model, slots, progress):
        count = len(self.search.tape.groups)
        rng_before = self.search.tape.rng.get_state().clone()
        try:
            actions, values, logs, imitation = super().choose(model, slots, progress)
        except AllocationEnded:
            del self.search.tape.groups[count:]
            self.search.tape.rng.set_state(rng_before)
            raise
        imitation = [imit and progress[s][PHASE] != COMBAT_PHASE for s, imit in zip(slots, imitation)]
        return actions, values, logs, imitation

    def state(self):
        state = super().state()
        state['combat_tape'] = self.search.tape.state()
        return state

    def restore(self, state):
        super().restore(state)
        self.search.tape.restore(state['combat_tape'])

    def collect(self, model, length=64):
        result = super().collect(model, length)
        result['combat_copies'] = self.search.tape.drain()
        return result
