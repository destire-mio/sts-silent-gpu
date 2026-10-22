from collections import Counter, deque
from copy import deepcopy
from dataclasses import dataclass, field
import json
import math
import time

import torch

from .model import FIELDS as views_fields, SCREENS, distribution, network_inputs, store_inputs, views
from .rollout import make_batch
from .e4_rewards import RewardHistory


FLOOR, ACT, PHASE, ROOM, SCREEN, KEYS, BATTLE_OVER, SMOKED, HP = range(9)
COMBAT, EVENT, MAP, ROOM_SCREEN, DEATH, VICTORY = 0, 8, 9, 10, 4, 3
COMBAT_PHASE, COMPLETE_PHASE = 1, 2
BOSS_ROOM, ELITE_ROOM, EVENT_ROOM, NEOW_ROOM = 2, 1, 6, 8
MAX_REWARD = 156
ACTION_KINDS = ("padding", "play", "end_turn", "choose", "potion_use", "potion_discard", "proceed", "skip")
FORK_METRICS = ("forks", "branches", "completed_branches", "capped_branches", "fork_all_capped", "score_gap_count",
                "score_gap_sum", "score_gap_max", "seconds", "searched_decisions")
SEARCH_METRICS = ("combat_over_limit", "event_over_limit", "combat_normal_max_steps", "event_normal_max_steps")
CAPACITY_METRICS = ("combat_played_cards_limit", "combat_trials")
LOOP_METRICS = ("repeat_check_seconds", "repeat_check_decisions", "repeat_check_candidates", "all_candidates_repeat")
REACH = ("act1_boss", "act2_boss", "act3_first_boss", "act3_second_boss",
         "act3_three_keys", "act4", "shield_spear", "heart")


COMBAT_DIAGNOSTIC_METRICS = ('policy_entropy', 'win_fraction', 'score_std', 'distinct_end_states', 'selected_score_gain')


def combat_diagnostics_summary(rows):
    result = {}
    for label, group in [('all', rows)] + [(f'act{act}', [r for r in rows if r['act'] == act]) for act in range(1, 5)]:
        decisions = sum(r['decision_count'] for r in group)
        result[label] = dict(combats=len(group), decisions=decisions,
            complete_trial_groups=sum(r['capped_trials'] == 0 for r in group),
            capped_trials=sum(r['capped_trials'] for r in group),
            nested_event_combats=sum(r['nested_event'] for r in group),
            pooled_policy_entropy=sum(r['entropy_sum'] for r in group)/decisions if decisions else None,
            means={key: (sum(r[key] for r in group if r[key] is not None)/sum(r[key] is not None for r in group)
                         if any(r[key] is not None for r in group) else None) for key in COMBAT_DIAGNOSTIC_METRICS})
    return result


class AllocationEnded(Exception):
    pass


@dataclass
class RewardLedger:
    floors: set = field(default_factory=set)
    bosses: set = field(default_factory=set)
    reach: dict = field(default_factory=lambda: dict.fromkeys(REACH, False))
    total: int = 0

    def advance(self, before, after):
        reward = 0
        if after[FLOOR] > before[FLOOR]:
            for floor in range(before[FLOOR] + 1, after[FLOOR] + 1):
                assert floor not in self.floors, "Duplicate floor reward"
                self.floors.add(floor)
                reward += 1
        if after[ACT] == 4 and not self.reach["act4"]:
            self.reach["act4"] = True
            reward += 10
        won = (before[PHASE] == COMBAT_PHASE and after[PHASE] != COMBAT_PHASE
               and after[BATTLE_OVER] and not after[SMOKED] and after[HP] > 0)
        if won and before[ROOM] == BOSS_ROOM:
            identity = (before[ACT], before[FLOOR])
            assert identity not in self.bosses, "Duplicate boss reward"
            self.bosses.add(identity)
            act = before[ACT]
            if act < 3:
                self.reach[f"act{act}_boss"] = True
                reward += 5
            elif act == 3:
                count = sum(a == 3 for a, _ in self.bosses)
                assert count in (1, 2)
                self.reach["act3_first_boss" if count == 1 else "act3_second_boss"] = True
                reward += 5
            else:
                self.reach["heart"] = True
                reward += 50
        if (won and before[ACT] == 4 and before[ROOM] == ELITE_ROOM
                and not self.reach["shield_spear"]):
            self.reach["shield_spear"] = True
            reward += 10
        if after[ACT] == 3 and self.reach["act3_second_boss"] and after[KEYS] == 7:
            self.reach["act3_three_keys"] = True
        self.total += reward
        assert self.total <= MAX_REWARD
        return reward / MAX_REWARD


class RoomSearch:
    def __init__(self, lanes, trials, device, rng, *, scratch_slots=4096, inference_chunk=128):
        self.lanes, self.trials, self.device, self.rng = lanes, trials, device, rng
        self.size = lanes + scratch_slots + 128
        self.batch = make_batch(self.size, 2**63 - 1, device)
        self.arrays = views(self.batch, device)
        for name in ("entity_counts", "candidate_counts", "faults", "done", "truncated"):
            self.arrays[name] = torch.as_tensor(getattr(self.batch, name), device=device)
        self.progress_buffer = torch.empty(self.size * 9, dtype=torch.int64, device=device)
        self.available = list(range(lanes, lanes + scratch_slots))
        self.probe_slots = list(range(lanes + scratch_slots, self.size))
        self.seen = {}
        self.mask_cache = {}
        self.frame_cache = {}
        self.reward_history = [RewardHistory() for _ in range(self.size)]
        self.loop_metrics = dict.fromkeys(LOOP_METRICS, 0)
        self.inference_chunk = inference_chunk
        self.deadline = float("inf")
        self.searched_decisions = 0
        self.event_trial_max_steps = 0
        self.provenance = {}
        self.evidence_directory = None
        self.metrics = dict.fromkeys(SEARCH_METRICS, 0)
        self.capacity_metrics = dict.fromkeys(CAPACITY_METRICS, 0)
        if device.type == "cuda":
            from spire_gpu import PLAYED_CARDS_CAPACITY_FAULT
        else:
            from spire import PLAYED_CARDS_CAPACITY_FAULT
        self.played_cards_capacity_fault = PLAYED_CARDS_CAPACITY_FAULT
        self.contexts = []
        self.last_heartbeat = time.monotonic()
        self.actual_lanes = []
        self.combat_tails = {}
        self.combat_diagnostics = None
        self.fork_metrics = dict.fromkeys(FORK_METRICS + SEARCH_METRICS + CAPACITY_METRICS + LOOP_METRICS, 0)
        self.map_returned = torch.zeros(self.size, dtype=torch.bool, device=device)

    def clock(self):
        now = time.monotonic()
        if now - self.last_heartbeat >= 60:
            print(json.dumps(dict(event="sl_heartbeat", device=self.device.index,
                lanes=self.actual_lanes, searches=[self.search_snapshot(c) for c in self.contexts])), flush=True)
            self.last_heartbeat = now
        if now >= self.deadline:
            raise AllocationEnded()

    def search_snapshot(self, context):
        slots = context["live"]
        longest = max(slots, key=lambda s: context["total_steps"][s]) if slots else None
        return dict(kind=context["kind"], unfinished=len(slots), slot=longest,
            steps=context["total_steps"][longest] if longest is not None else 0,
            noncombat_steps=context["steps"][longest] if longest is not None and context["kind"] == "event" else None,
            last30=list(context["tail"][longest]) if longest is not None else [])

    def begin_search(self, kind, slots):
        context = dict(kind=kind, live=slots[:], steps=dict.fromkeys(slots, 0), total_steps=dict.fromkeys(slots, 0),
                       tail={s: deque(maxlen=30) for s in slots})
        self.contexts.append(context)
        return context

    def trace_step(self, context, slots, choices):
        self.batch.progress_tensors(self.progress_buffer)
        screens = self.progress_buffer.view(-1, 9)[slots, SCREEN].cpu().tolist()
        kinds = self.arrays["candidate_kinds"][slots, choices].cpu().tolist()
        for s, action, screen, kind in zip(slots, choices, screens, kinds):
            context["steps"][s] += 1
            context["total_steps"][s] += 1
            context["tail"][s].append(dict(screen=SCREENS[screen], candidate=action, action_kind=ACTION_KINDS[kind]))

    def exceeded(self, context, slots):
        for s in slots:
            self.metrics[context["kind"] + "_over_limit"] += 1
            record = dict(event="sl_over_limit", device=self.device.index, kind=context["kind"],
                slot=s, seed=self.provenance[s][0], steps=context["total_steps"][s],
                limit_steps=context["steps"][s],
                last30=list(context["tail"][s]))
            print(json.dumps(record), flush=True)
            if self.evidence_directory is not None:
                with (self.evidence_directory / f"sl-over-limit-{self.device.index}.jsonl").open("a") as out:
                    out.write(json.dumps(record) + "\n")

    def progress(self):
        self.batch.progress_tensors(self.progress_buffer)
        return self.progress_buffer.view(-1, 9).cpu().tolist()

    def check(self, slots, combat_context=None):
        faults = self.arrays["faults"].to(torch.int64)[slots].cpu().tolist()
        capped = set()
        if combat_context is not None:
            assert combat_context["kind"] == "combat"
            assert all(s in combat_context["live"] and s >= self.lanes for s in slots)
            capped = {s for s, f in zip(slots, faults) if f == self.played_cards_capacity_fault}
        unexpected = [(s, f) for s, f in zip(slots, faults) if f and s not in capped]
        if unexpected and self.evidence_directory is not None:
            evidence = [dict(slot=s, fault=f, seed=self.provenance[s][0],
                             candidates=self.provenance[s][1]) for s, f in unexpected]
            (self.evidence_directory / f"fault-device-{self.device.index}.json").write_text(json.dumps(evidence))
        assert not unexpected, f"E4 engine fault: {unexpected}"
        assert not bool(self.arrays["truncated"][slots].any()), "Unresolved search truncation"
        for s in sorted(capped):
            self.capacity_metrics["combat_played_cards_limit"] += 1
            record = dict(event="combat_played_cards_limit", device=self.device.index,
                slot=s, seed=self.provenance[s][0], fault=self.played_cards_capacity_fault,
                steps=combat_context["total_steps"][s], last30=list(combat_context["tail"][s]))
            print(json.dumps(record), flush=True)
            if self.evidence_directory is not None:
                with (self.evidence_directory / f"combat-played-cards-limit-{self.device.index}.jsonl").open("a") as out:
                    out.write(json.dumps(record) + "\n")
        return capped

    @torch.no_grad()
    def predict(self, model, slots):
        choices, values, logs = [], [], []
        if not self.contexts:
            self.actual_policy_logs = []
        masks = self.policy_masks(slots)
        offset = 0
        for group in torch.tensor(slots, device=self.device).split(self.inference_chunk):
            inputs = network_inputs({k: self.arrays[k] for k in views_fields}, self.device, group)
            inputs["candidate_mask"] &= masks[offset:offset + len(group), :inputs["candidate_mask"].shape[1]]
            offset += len(group)
            logits, value = model(inputs)
            policy, _ = distribution(logits, inputs["candidate_mask"])
            action = torch.multinomial(policy.probs, 1, generator=self.rng).flatten()
            if (self.combat_diagnostics is not None and self.contexts
                    and self.contexts[-1]["kind"] == "combat"):
                context = self.contexts[-1]
                for slot, entropy in zip(group.cpu().tolist(), policy.entropy().cpu().tolist()):
                    context["entropy_sum"][slot] += entropy
                    context["entropy_count"][slot] += 1
            choices.extend(action.cpu().tolist())
            values.extend(value.cpu().tolist())
            logs.extend(policy.log_prob(action).cpu().tolist())
            if not self.contexts:
                self.actual_policy_logs.extend(policy.logits.cpu().tolist())
        return choices, values, logs

    def loop_keys(self, slots):
        indices = torch.tensor(slots, dtype=torch.int64, device=self.device)
        keys = torch.empty(len(slots), dtype=torch.int64, device=self.device)
        self.batch.loop_keys_tensors(indices, keys)
        return keys.cpu().tolist()

    def reset_seen(self, slots):
        for s, key in zip(slots, self.loop_keys(slots)):
            self.seen[s] = {key}
            self.mask_cache.pop(s, None)

    def frames(self, slots, progress):
        missing = [s for s in slots if s not in self.frame_cache]
        if missing:
            data = {name: self.arrays[name][missing].cpu().tolist() for name in
                    ('tags', 'ids', 'values', 'candidate_kinds', 'candidate_refs', 'entity_counts', 'candidate_counts')}
            for j, s in enumerate(missing):
                tags, ids, values = data['tags'][j], data['ids'][j], data['values'][j]
                entities = range(data['entity_counts'][j])
                rewards = [i for i in entities if tags[0][i] == 12]
                reward_indices = {i + 1: r for r, i in enumerate(rewards)}
                refs = data['candidate_refs'][j][2]
                nc = data['candidate_counts'][j]
                self.frame_cache[s] = dict(screen=progress[s][SCREEN], floor=progress[s][FLOOR],
                    rewards=[(ids[0][i], ids[3][i], values[0][i]) for i in rewards],
                    actions=[(data['candidate_kinds'][j][a], reward_indices.get(refs[a])) for a in range(nc)],
                    rest_options=[ids[3][refs[a]-1] if refs[a] and tags[0][refs[a]-1] == 11
                                  and ids[0][refs[a]-1] == 5 else None for a in range(nc)],
                    empty_potion_slot=any(tags[0][i] == 7 and tags[1][i] == 0 and ids[0][i] == 1 for i in entities),
                    potion_count=sum(tags[0][i] == 7 and tags[1][i] == 0 and ids[0][i] != 1 for i in entities),
                    relic_count=sum(tags[0][i] == 6 and tags[1][i] == 0 for i in entities),
                    gold=next(values[2][i] for i in entities if tags[0][i] == 2),
                    deck_size=sum(tags[0][i] == 3 and tags[1][i] == 1 for i in entities))
        return {s: self.frame_cache[s] for s in slots}

    @torch.no_grad()
    def policy_masks(self, slots):
        missing = [s for s in slots if s not in self.mask_cache]
        if missing:
            started = time.monotonic()
            progress = self.progress()
            inputs = network_inputs({k: self.arrays[k] for k in views_fields}, self.device,
                                    torch.tensor(missing, device=self.device))
            legal = inputs["candidate_mask"] & ~(self.map_return_blocked(missing)[:, None]
                                                & (inputs["candidate_kinds"] == 7))
            assert bool(legal.any(-1).all()), "Map-return restriction removed every action"
            legal = legal.cpu().tolist()
            frames = self.frames([s for s in missing if progress[s][PHASE] != COMBAT_PHASE], progress)
            pairs = []
            allowed = {}
            for s, row in zip(missing, legal):
                if s in frames:
                    nc = len(frames[s]['actions'])
                    masked, _ = self.reward_history[s].mask(frames[s], row[:nc])
                    row = masked + [False] * (len(row) - nc)
                    assert any(row), 'Reward restriction removed every action'
                allowed[s] = row + [False] * (128 - len(row))
                if progress[s][PHASE] != COMBAT_PHASE:
                    pairs.extend((s, a) for a, valid in enumerate(row) if valid)
            repeated = {s: [] for s in missing}
            for begin in range(0, len(pairs), len(self.probe_slots)):
                self.clock()
                group = pairs[begin:begin + len(self.probe_slots)]
                probes = self.probe_slots[:len(group)]
                self.batch.clone_many([s for s, _ in group], probes)
                selected = torch.zeros(self.size, dtype=torch.int32, device=self.device)
                active = torch.zeros(self.size, dtype=torch.uint8, device=self.device)
                selected[probes] = torch.tensor([a for _, a in group], dtype=torch.int32, device=self.device)
                active[probes] = 1
                for (s, a), p in zip(group, probes):
                    seed, path = self.provenance[s]
                    self.provenance[p] = (seed, path + [a])
                self.batch.step_tensors(selected, active)
                self.check(probes)
                keys = self.loop_keys(probes)
                self.batch.progress_tensors(self.progress_buffer)
                floors = self.progress_buffer.view(-1, 9)[probes, FLOOR].cpu().tolist()
                for (s, a), key, floor in zip(group, keys, floors):
                    if floor == progress[s][FLOOR] and key in self.seen[s]:
                        repeated[s].append(a)
            for s in missing:
                row = allowed[s]
                if repeated[s] and len(repeated[s]) == sum(row):
                    self.loop_metrics["all_candidates_repeat"] += 1
                else:
                    for a in repeated[s]:
                        row[a] = False
                self.mask_cache[s] = row
            if pairs:
                self.loop_metrics["repeat_check_seconds"] += time.monotonic() - started
                self.loop_metrics["repeat_check_decisions"] += sum(progress[s][PHASE] != COMBAT_PHASE for s in missing)
                self.loop_metrics["repeat_check_candidates"] += len(pairs)
        return torch.tensor([self.mask_cache[s] for s in slots], dtype=torch.bool, device=self.device)

    def map_return_blocked(self, slots):
        self.batch.progress_tensors(self.progress_buffer)
        return self.map_returned[slots] & (self.progress_buffer.view(-1, 9)[slots, SCREEN] == MAP)

    @torch.no_grad()
    def scores(self, model, slots, progress):
        result = dict.fromkeys(slots, 0.)
        alive = [s for s in slots if progress[s][SCREEN] != DEATH]
        for group in torch.tensor(alive, device=self.device, dtype=torch.int64).split(self.inference_chunk):
            if not len(group):
                continue
            inputs = network_inputs({k: self.arrays[k] for k in views_fields}, self.device, group)
            _, values = model(inputs)
            result.update(zip(group.cpu().tolist(), values.cpu().tolist()))
        return result

    def step(self, slots, choices, combat_context=None):
        indices = torch.tensor(slots, device=self.device, dtype=torch.int64)
        self.batch.progress_tensors(self.progress_buffer)
        before = self.progress_buffer.view(-1, 9)[indices].clone()
        before_progress = dict(zip(slots, before.cpu().tolist()))
        tracked = [s for s in slots if before_progress[s][PHASE] != COMBAT_PHASE]
        frames_before = self.frames(tracked, before_progress)
        action_by_slot = dict(zip(slots, choices))
        for s in tracked:
            frame = frames_before[s]
            self.reward_history[s].before_step(frame, action_by_slot[s], [True] * len(frame['actions']))
        returning = (before[:, SCREEN] == MAP) & (self.arrays["candidate_kinds"][indices, choices] == 7)
        assert not bool((returning & self.map_returned[indices]).any()), "Second map return on the same floor"
        self.map_returned[indices] |= returning
        selected = torch.zeros(self.size, dtype=torch.int32, device=self.device)
        active = torch.zeros(self.size, dtype=torch.uint8, device=self.device)
        selected[indices] = torch.tensor(choices, device=self.device, dtype=torch.int32)
        active[indices] = 1
        self.batch.step_tensors(selected, active)
        self.batch.progress_tensors(self.progress_buffer)
        self.map_returned[indices] &= before[:, FLOOR] == self.progress_buffer.view(-1, 9)[indices, FLOOR]
        for s, a in zip(slots, choices):
            self.provenance[s][1].append(a)
        capped = self.check(slots, combat_context)
        floors_before = before[:, FLOOR].cpu().tolist()
        floors_after = self.progress_buffer.view(-1, 9)[indices, FLOOR].cpu().tolist()
        after_progress = dict(zip(slots, self.progress_buffer.view(-1, 9)[indices].cpu().tolist()))
        for s in slots:
            self.frame_cache.pop(s, None)
        observed = [s for s in slots if s not in capped and
                    (s in frames_before or after_progress[s][PHASE] != COMBAT_PHASE)]
        frames_after = self.frames(observed, after_progress)
        for s in observed:
            if (before_progress[s][PHASE] != COMBAT_PHASE and after_progress[s][PHASE] == COMBAT_PHASE
                    and after_progress[s][ROOM] == BOSS_ROOM):
                counts = self.reward_history[s].counts
                label = f'boss_act{after_progress[s][ACT]}'
                counts[label + '_arrivals'] += 1
                counts[label + '_deck_size_sum'] += frames_after[s]['deck_size']
            if s in frames_before:
                self.reward_history[s].after_step(frames_before[s], frames_after[s], action_by_slot[s])
            else:
                self.reward_history[s].observe(frames_after[s])
        valid = [s for s in slots if s not in capped]
        keys = dict(zip(valid, self.loop_keys(valid))) if valid else {}
        for s, old, new in zip(slots, floors_before, floors_after):
            if s in capped:
                continue
            if old != new:
                self.seen[s] = set()
            self.seen[s].add(keys[s])
            self.mask_cache.pop(s, None)
        return capped

    def clone(self, sources, targets):
        self.batch.clone_many(sources, targets)
        self.map_returned[targets] = self.map_returned[sources]
        for s, t in zip(sources, targets):
            seed, path = self.provenance[s]
            self.provenance[t] = (seed, path.copy())
            self.seen[t] = self.seen[s].copy()
            self.reward_history[t] = deepcopy(self.reward_history[s])
            self.frame_cache.pop(t, None)
            if s in self.mask_cache:
                self.mask_cache[t] = self.mask_cache[s].copy()
            else:
                self.mask_cache.pop(t, None)
        self.check(targets)

    def take(self, count):
        assert count <= len(self.available)
        slots = self.available[-count:]
        del self.available[-count:]
        return slots

    def combat(self, model, sources):
        return self._combat(model, sources, self.trials)

    def _combat(self, model, sources, trials):
        selected_paths = {}
        capacity = len(self.available) // trials
        assert capacity > 0
        for begin in range(0, len(sources), capacity):
            parents = sources[begin:begin + capacity]
            slots = self.take(len(parents) * trials)
            context = self.begin_search("combat", slots)
            if self.combat_diagnostics is not None:
                starts = self.progress()
                context["starts"] = {p: (starts[p][ACT], starts[p][FLOOR], self.provenance[p][0]) for p in parents}
                context["entropy_sum"] = dict.fromkeys(slots, 0.)
                context["entropy_count"] = dict.fromkeys(slots, 0)
            try:
                self.clone([s for s in parents for _ in range(trials)], slots)
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
                scores = self.scores(model, eligible, progress)
                if self.combat_diagnostics is not None:
                    keys = dict(zip(eligible, self.loop_keys(eligible))) if eligible else {}
                    for j, parent in enumerate(parents):
                        trial_slots = slots[j * trials:(j + 1) * trials]
                        group = [s for s in trial_slots if s not in capped]
                        act, floor, seed = context["starts"][parent]
                        count = sum(context["entropy_count"][s] for s in trial_slots)
                        entropy_sum = sum(context["entropy_sum"][s] for s in trial_slots)
                        row = dict(act=act, floor=floor, seed=seed, trials=len(trial_slots),
                            capped_trials=len(trial_slots)-len(group), decision_count=count,
                            entropy_sum=entropy_sum, policy_entropy=entropy_sum/count,
                            nested_event=any(c["kind"] == "event" for c in self.contexts),
                            win_fraction=None, score_std=None, distinct_end_states=None, selected_score_gain=None)
                        if len(group) == len(trial_slots):
                            mean = sum(scores[s] for s in group)/len(group)
                            row.update(win_fraction=sum(progress[s][BATTLE_OVER] and not progress[s][SMOKED]
                                and progress[s][HP] > 0 for s in group)/len(group),
                                score_std=math.sqrt(sum((scores[s]-mean)**2 for s in group)/len(group)),
                                distinct_end_states=len({keys[s] for s in group}),
                                selected_score_gain=max(scores[s] for s in group)-mean)
                        self.combat_diagnostics.append(row)
                for j, parent in enumerate(parents):
                    group = [s for s in slots[j * trials:(j + 1) * trials] if s not in capped]
                    if group:
                        winner = max(group, key=scores.get)
                        self.clone([winner], [parent])
                        selected_paths[parent] = paths[winner]
                        self.combat_tails[parent] = list(context["tail"][winner])
                    else:
                        selected_paths[parent] = None
            finally:
                self.contexts.pop()
                self.available.extend(slots)
        return selected_paths

    @staticmethod
    def event_exit(p):
        return (p[SCREEN] in (MAP, DEATH, VICTORY)
                or p[ROOM] not in (EVENT_ROOM, NEOW_ROOM, 11)
                or (p[SCREEN] == ROOM_SCREEN and p[PHASE] == COMPLETE_PHASE))

    def event(self, model, sources):
        options = []
        parents = []
        groups = []
        masks = self.policy_masks(sources).cpu().tolist()
        for parent, mask in zip(sources, masks):
            kinds = self.arrays["candidate_kinds"][parent].cpu().tolist()
            choices = [i for i, kind in enumerate(kinds) if kind == 3 and mask[i]]
            assert choices
            groups.append((len(options), len(options) + len(choices)))
            options.extend(choices)
            parents.extend([parent] * len(choices))
        slots = self.take(len(options))
        paths = {s: [a] for a, s in zip(options, slots)}
        context = self.begin_search("event", slots)
        capped = set()
        try:
            self.clone(parents, slots)
            self.trace_step(context, slots, options)
            self.step(slots, options)
            self.searched_decisions += len(slots)
            live = slots[:]
            while live:
                context["live"] = live
                self.clock()
                progress = self.progress()
                finished = [s for s in live if self.event_exit(progress[s])]
                for s in finished:
                    self.metrics["event_normal_max_steps"] = max(self.metrics["event_normal_max_steps"], len(paths[s]))
                live = [s for s in live if s not in finished]
                expired = [s for s in live if context["steps"][s] > 500]
                self.exceeded(context, expired)
                capped.update(expired)
                live = [s for s in live if s not in capped]
                fighting = [s for s in live if progress[s][PHASE] == COMBAT_PHASE]
                if fighting:
                    branches = self.combat(model, fighting)
                    for s, actions in branches.items():
                        if actions is None:
                            capped.add(s)
                        else:
                            paths[s].extend(actions)
                            context["total_steps"][s] += len(actions)
                            context["tail"][s].extend(self.combat_tails[s])
                    live = [s for s in live if s not in capped]
                    continue
                if live:
                    choices, _, _ = self.predict(model, live)
                    self.trace_step(context, live, choices)
                    self.step(live, choices)
                    self.searched_decisions += len(live)
                    for s, a in zip(live, choices):
                        paths[s].append(a)
            progress = self.progress()
            scores = self.scores(model, [s for s in slots if s not in capped], progress)
            selected = {}
            for parent, (begin, end) in zip(sources, groups):
                eligible = [s for s in slots[begin:end] if s not in capped]
                if eligible:
                    winner = max(eligible, key=scores.get)
                    self.clone([winner], [parent])
                    selected[parent] = paths[winner]
                else:
                    selected[parent] = None
            return selected
        finally:
            self.event_trial_max_steps = max(self.event_trial_max_steps, max(map(len, paths.values())))
            self.contexts.pop()
            self.available.extend(slots)

    def fork_options(self, sources, progress):
        if not sources:
            return {}
        frames = self.frames(sources, progress)
        masks = self.policy_masks(sources).cpu().tolist()
        result = {}
        for parent, mask in zip(sources, masks):
            p, frame = progress[parent], frames[parent]
            if p[PHASE] == COMBAT_PHASE:
                continue
            screen = p[SCREEN]
            choices = [a for a, (kind, _) in enumerate(frame['actions']) if mask[a] and
                       ((screen in (6, 12) and kind in (3, 7)) or
                        (screen == ROOM_SCREEN and kind == 3 and frame['rest_options'][a] is not None))]
            if len(choices) > 1:
                result[parent] = choices
        return result

    def fork(self, model, options, rewards, floor_steps, noncombat_floor_steps):
        started = time.monotonic()
        rng_before = self.rng.get_state()
        metrics_before, capacity_before = self.metrics.copy(), self.capacity_metrics.copy()
        loops_before, searched_before = self.loop_metrics.copy(), self.searched_decisions
        diagnostics_before = self.combat_diagnostics
        self.combat_diagnostics = None
        self.metrics = dict.fromkeys(SEARCH_METRICS, 0)
        self.capacity_metrics = dict.fromkeys(CAPACITY_METRICS, 0)
        self.loop_metrics = dict.fromkeys(LOOP_METRICS, 0)
        self.searched_decisions = 0
        parents = [p for p, choices in options.items() for _ in choices]
        choices = [a for choices in options.values() for a in choices]
        slots = self.take(len(choices))
        context = self.begin_search("event", slots)
        capped, scores = set(), {}
        ledgers = {s: deepcopy(rewards[p]) for s, p in zip(slots, parents)}
        earned, battles = dict.fromkeys(slots, 0.), dict.fromkeys(slots, 0)
        steps = {s: floor_steps[p] for s, p in zip(slots, parents)}
        noncombat = {s: noncombat_floor_steps[p] for s, p in zip(slots, parents)}
        self.fork_metrics["forks"] += len(options)
        self.fork_metrics["branches"] += len(slots)

        def advance(active, before, counts):
            after = self.progress()
            expired, finished = [], []
            for slot in active:
                old, new = before[slot], after[slot]
                earned[slot] += ledgers[slot].advance(old, new)
                finished_combat = (old[PHASE] == COMBAT_PHASE and new[PHASE] != COMBAT_PHASE
                                   and new[BATTLE_OVER] and not new[SMOKED])
                battles[slot] += int(finished_combat)
                terminal = new[SCREEN] in (DEATH, VICTORY)
                if old[FLOOR] != new[FLOOR]:
                    steps[slot], noncombat[slot] = 0, 0
                else:
                    steps[slot] += counts[slot]
                    noncombat[slot] += counts[slot] if old[PHASE] != COMBAT_PHASE else 0
                if not terminal and (steps[slot] > 3000 or noncombat[slot] > 200
                                     or context["steps"][slot] > 500):
                    expired.append(slot)
                elif terminal or battles[slot] >= 2 or (finished_combat and old[ROOM] == BOSS_ROOM):
                    finished.append(slot)
            self.exceeded(context, expired)
            capped.update(expired)
            bootstrap = self.scores(model, [s for s in finished if after[s][SCREEN] not in (DEATH, VICTORY)], after)
            for slot in finished:
                scores[slot] = earned[slot] + bootstrap.get(slot, 0.)
                self.metrics["event_normal_max_steps"] = max(
                    self.metrics["event_normal_max_steps"], context["total_steps"][slot])

        try:
            self.clone(parents, slots)
            before = self.progress()
            self.trace_step(context, slots, choices)
            self.step(slots, choices)
            self.searched_decisions += len(slots)
            advance(slots, before, dict.fromkeys(slots, 1))
            live = [s for s in slots if s not in capped and s not in scores]
            while live:
                context["live"] = live
                self.clock()
                before = self.progress()
                fighting = [s for s in live if before[s][PHASE] == COMBAT_PHASE]
                if fighting:
                    paths = self._combat(model, fighting, 8)
                    counts = {}
                    for slot, path in paths.items():
                        if path is None:
                            capped.add(slot)
                        else:
                            counts[slot] = len(path)
                            context["total_steps"][slot] += len(path)
                            context["tail"][slot].extend(self.combat_tails[slot])
                    advance(list(counts), before, counts)
                walking = [s for s in live if s not in fighting]
                if walking:
                    picked, _, _ = self.predict(model, walking)
                    self.trace_step(context, walking, picked)
                    self.step(walking, picked)
                    self.searched_decisions += len(walking)
                    advance(walking, before, dict.fromkeys(walking, 1))
                live = [s for s in live if s not in capped and s not in scores]
            self.fork_metrics["completed_branches"] += len(scores)
            self.fork_metrics["capped_branches"] += len(capped)
            selected = {}
            for parent, parent_choices in options.items():
                group = [(s, a) for s, p, a in zip(slots, parents, choices) if p == parent and s in scores]
                if group:
                    winner, action = max(group, key=lambda pair: scores[pair[0]])
                    selected[parent] = action
                    gap = max(scores[s] for s, _ in group) - min(scores[s] for s, _ in group)
                    self.fork_metrics["score_gap_count"] += 1
                    self.fork_metrics["score_gap_sum"] += gap
                    self.fork_metrics["score_gap_max"] = max(self.fork_metrics["score_gap_max"], gap)
                else:
                    selected[parent] = None
                    self.fork_metrics["fork_all_capped"] += 1
            return selected
        finally:
            self.contexts.pop()
            self.available.extend(slots)
            self.rng.set_state(rng_before)
            self.fork_metrics["seconds"] += time.monotonic() - started
            self.fork_metrics["searched_decisions"] += self.searched_decisions
            for key in SEARCH_METRICS:
                if key.endswith("max_steps"):
                    self.fork_metrics[key] = max(self.fork_metrics[key], self.metrics[key])
                else:
                    self.fork_metrics[key] += self.metrics[key]
            for key in CAPACITY_METRICS:
                self.fork_metrics[key] += self.capacity_metrics[key]
            for key in LOOP_METRICS:
                self.fork_metrics[key] += self.loop_metrics[key]
            self.metrics, self.capacity_metrics = metrics_before, capacity_before
            self.loop_metrics, self.searched_decisions = loops_before, searched_before
            self.combat_diagnostics = diagnostics_before

    def plan(self, model, sources, kind):
        backups = self.take(len(sources))
        self.clone(sources, backups)
        rng_before = self.rng.get_state()
        diagnostics_before = len(self.combat_diagnostics) if self.combat_diagnostics is not None else 0
        try:
            return self.combat(model, sources) if kind == "combat" else self.event(model, sources)
        except AllocationEnded:
            self.rng.set_state(rng_before)
            if self.combat_diagnostics is not None:
                del self.combat_diagnostics[diagnostics_before:]
            raise
        finally:
            self.clone(backups, sources)
            self.available.extend(backups)


class SegmentCollector:
    def __init__(self, search, seed_rng, *, rank=0, fork_rng=None):
        self.search, self.seed_rng, self.rank = search, seed_rng, rank
        self.fork_rng = fork_rng
        self.seeds = [None] * search.lanes
        self.paths = [[] for _ in self.seeds]
        self.floor_steps = [0] * search.lanes
        self.noncombat_floor_steps = [0] * search.lanes
        self.tails = [deque(maxlen=10) for _ in self.seeds]
        self.last_stall_classes = {}
        self.pending = [deque() for _ in self.seeds]
        self.rewards = [RewardLedger() for _ in self.seeds]
        self.completed = []
        self.decisions = 0
        self.partial = None
        self.evaluation_waiting = None
        self.stalled = set()

    def reset(self, slots, seeds):
        for s, seed in zip(slots, seeds):
            self.search.provenance[s] = (seed, [])
        self.search.batch.reset_to_many(slots, seeds, [[] for _ in slots])
        self.search.map_returned[slots] = False
        for s in slots:
            self.search.reward_history[s] = RewardHistory()
            self.search.frame_cache.pop(s, None)
        self.search.check(slots)
        self.search.reset_seen(slots)
        for slot, seed in zip(slots, seeds):
            self.seeds[slot] = seed
            self.paths[slot] = []
            self.floor_steps[slot] = 0
            self.noncombat_floor_steps[slot] = 0
            self.tails[slot].clear()
            self.pending[slot].clear()
            self.rewards[slot] = RewardLedger()

    def fresh(self, slots):
        seeds = torch.randint(1000000000, 2000000000, (len(slots),),
                              generator=self.seed_rng).tolist()
        self.reset(slots, seeds)

    def state(self):
        return dict(seeds=self.seeds, paths=self.paths, floor_steps=self.floor_steps,
                    reward_history=deepcopy(self.search.reward_history[:self.search.lanes]),
                    noncombat_floor_steps=self.noncombat_floor_steps, tails=[list(t) for t in self.tails],
                    map_returned=self.search.map_returned[:self.search.lanes].cpu().tolist(),
                    seen={s: self.search.seen[s] for s in range(self.search.lanes) if s in self.search.seen},
                    last_stall_classes=self.last_stall_classes,
                    pending=[list(p) for p in self.pending], rewards=self.rewards,
                    seed_rng=self.seed_rng.get_state(), search_rng=self.search.rng.get_state(),
                    fork_rng=self.fork_rng.get_state() if self.fork_rng is not None else None,
                    decisions=self.decisions, completed=self.completed, partial=self.partial,
                    evaluation_waiting=self.evaluation_waiting)

    def restore(self, state):
        for name in ("seeds", "paths", "floor_steps", "noncombat_floor_steps", "last_stall_classes",
                     "rewards", "decisions", "completed", "partial", "evaluation_waiting"):
            setattr(self, name, state[name])
        self.tails = [deque(t, maxlen=10) for t in state["tails"]]
        self.search.map_returned[:self.search.lanes] = torch.tensor(state["map_returned"], device=self.search.device)
        self.search.seen = {s: keys.copy() for s, keys in state["seen"].items()}
        self.search.mask_cache.clear()
        self.search.frame_cache.clear()
        self.search.reward_history[:self.search.lanes] = deepcopy(state["reward_history"])
        if self.partial:
            for name, value in self.partial["result"].items():
                self.partial["result"][name] = value.to(self.search.device)
        self.pending = [deque(p) for p in state["pending"]]
        if self.fork_rng is not None:
            self.fork_rng.set_state(state["fork_rng"])
        self.seed_rng.set_state(state["seed_rng"])
        self.search.rng.set_state(state["search_rng"])
        slots = [i for i, seed in enumerate(self.seeds) if seed is not None]
        if slots:
            for s in slots:
                self.search.provenance[s] = (self.seeds[s], self.paths[s].copy())
            self.search.batch.reset_to_many(slots, [self.seeds[i] for i in slots],
                                            [self.paths[i] for i in slots])
            self.search.check(slots)

    def choose(self, model, slots, progress, *, training=False):
        rng_before = self.search.rng.get_state()
        fork_before = self.fork_rng.get_state() if self.fork_rng is not None else None
        pending_before = deepcopy(self.pending)
        try:
            return self._choose(model, slots, progress, training=training)
        except AllocationEnded:
            self.search.rng.set_state(rng_before)
            if self.fork_rng is not None:
                self.fork_rng.set_state(fork_before)
            self.pending = pending_before
            raise

    def _choose(self, model, slots, progress, *, training):
        self.stalled = set()
        self.search.actual_lanes = [dict(lane=s, seed=self.seeds[s], steps=len(self.paths[s]),
            floor=progress[s][FLOOR], screen=SCREENS[progress[s][SCREEN]]) for s in slots]
        actions, values, logs = self.search.predict(model, slots)
        action_logs = self.search.actual_policy_logs
        chosen = dict(zip(slots, actions))
        idle = [s for s in slots if not self.pending[s]]
        combat = [s for s in idle if progress[s][PHASE] == COMBAT_PHASE]
        events = [s for s in idle if progress[s][PHASE] != COMBAT_PHASE and progress[s][SCREEN] == EVENT
                  and int(self.search.arrays["candidate_kinds"][s, chosen[s]]) == 3]
        if combat:
            for s, path in self.search.plan(model, combat, "combat").items():
                if path is None:
                    self.stalled.add(s)
                else:
                    self.pending[s].extend(path)
        if events:
            for s, path in self.search.plan(model, events, "event").items():
                if path is None:
                    self.stalled.add(s)
                else:
                    self.pending[s].append(path[0])
        if training and self.fork_rng is not None:
            options = self.search.fork_options([s for s in idle if s not in combat and s not in events], progress)
            selected = {s: choices for s, choices in options.items()
                        if float(torch.rand((), generator=self.fork_rng)) < .25}
            if selected:
                for s, action in self.search.fork(model, selected, self.rewards,
                                                 self.floor_steps, self.noncombat_floor_steps).items():
                    if action is not None:
                        self.pending[s].append(action)
        imitation = []
        for j, s in enumerate(slots):
            imitation.append(bool(self.pending[s]))
            if self.pending[s]:
                actions[j] = self.pending[s].popleft()
                logs[j] = action_logs[j][actions[j]]
        return actions, values, logs, imitation

    def advance(self, slots, actions, progress):
        active = [(s, a) for s, a in zip(slots, actions) if s not in self.stalled]
        if active:
            kinds = self.search.arrays["candidate_kinds"][[s for s, _ in active],
                                                        [a for _, a in active]].cpu().tolist()
            for (s, _), kind in zip(active, kinds):
                self.tails[s].append((SCREENS[progress[s][SCREEN]], ACTION_KINDS[kind]))
            self.search.step([s for s, _ in active], [a for _, a in active])
        after = self.search.progress()
        reward, terminal = [], []
        for slot, action in zip(slots, actions):
            stalled = slot in self.stalled
            stall_reason = "all_sl_trials_capped" if stalled else None
            if not stalled:
                self.paths[slot].append(action)
                self.decisions += 1
                self.floor_steps[slot] = (self.floor_steps[slot] + 1
                    if progress[slot][FLOOR] == after[slot][FLOOR] else 0)
                self.noncombat_floor_steps[slot] = (self.noncombat_floor_steps[slot]
                    + int(progress[slot][PHASE] != COMBAT_PHASE)
                    if progress[slot][FLOOR] == after[slot][FLOOR] else 0)
                if after[slot][SCREEN] not in (DEATH, VICTORY) and (
                        self.floor_steps[slot] > 3000 or self.noncombat_floor_steps[slot] > 200):
                    stalled = True
                    stall_reason = ("noncombat_floor_limit" if self.noncombat_floor_steps[slot] > 200
                                    else "total_floor_limit")
                    self.pending[slot].clear()
            reward.append(0. if stalled else self.rewards[slot].advance(progress[slot], after[slot]))
            end = stalled or after[slot][SCREEN] in (DEATH, VICTORY)
            terminal.append(end)
            if end:
                assert not self.pending[slot]
                stall_class = None
                if stalled:
                    tail = list(self.tails[slot])
                    stall_class = json.dumps(dict(reason=stall_reason, last10=tail), separators=(",", ":"))
                    record = dict(event="stalled_episode", rank=self.rank, seed=self.seeds[slot],
                        floor=after[slot][FLOOR], reason=stall_reason, last10=tail,
                        floor_steps=self.floor_steps[slot], noncombat_floor_steps=self.noncombat_floor_steps[slot])
                    print(json.dumps(record), flush=True)
                    if self.search.evidence_directory is not None:
                        with (self.search.evidence_directory / f"stalled-{self.rank}.jsonl").open("a") as out:
                            out.write(json.dumps(record) + "\n")
                self.completed.append(dict(seed=self.seeds[slot], candidates=self.paths[slot],
                    reach=self.rewards[slot].reach.copy(), floor=after[slot][FLOOR],
                    stall_class=stall_class,
                    noncombat=dict(counts=dict(self.search.reward_history[slot].counts)),
                    final_deck_size=self.search.frames([slot], after)[slot]["deck_size"],
                    outcome="stalled" if stalled else "victory" if after[slot][SCREEN] == VICTORY else "death",
                    reward=self.rewards[slot].total / MAX_REWARD))
                self.seeds[slot] = None
        return reward, terminal

    @torch.no_grad()
    def collect(self, model, length=64):
        device, lanes = self.search.device, self.search.lanes
        slots = list(range(lanes))
        index = torch.arange(lanes, device=device)
        if self.partial is None:
            result = dict(data=torch.empty(length * lanes * 32768, dtype=torch.uint8, device=device),
                          offsets=torch.zeros(length * lanes + 1, dtype=torch.int64, device=device),
                          policy_mask=torch.empty((length, lanes, 128), dtype=torch.bool, device=device))
            for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "action_valid"):
                dtype = torch.int64 if name == "actions" else torch.bool if name in ("terminal", "imitation", "action_valid") else torch.float32
                result[name] = torch.zeros((length, lanes), device=device, dtype=dtype)
            self.partial = dict(result=result, next_step=0, length=length,
                                completed_before=len(self.completed))
        assert self.partial["length"] == length
        result = self.partial["result"]
        for t in range(self.partial["next_step"], length):
            self.search.clock()
            fresh = [s for s in slots if self.seeds[s] is None]
            if fresh:
                self.fresh(fresh)
            progress = self.search.progress()
            actions, values, logs, imitation = self.choose(model, slots, progress, training=True)
            result["policy_mask"][t] = self.search.policy_masks(slots)
            row = t * lanes
            store_inputs(self.search.batch, self.search.arrays, index, result["data"],
                         result["offsets"][row:row + lanes + 1])
            reward, terminal = self.advance(slots, actions, progress)
            for name, data in (("actions", actions), ("values", values), ("logs", logs),
                               ("rewards", reward), ("terminal", terminal), ("imitation", imitation),
                               ("action_valid", [s not in self.stalled for s in slots])):
                result[name][t] = torch.tensor(data, device=device)
            self.partial["next_step"] = t + 1
        live = [s for s in slots if self.seeds[s] is not None]
        bootstrap = torch.zeros(lanes, device=device)
        if live:
            values = self.search.scores(model, live, self.search.progress())
            bootstrap[live] = torch.tensor([values[s] for s in live], device=device)
        result["advantages"] = torch.zeros_like(result["values"])
        advantage = torch.zeros(lanes, device=device)
        for t in reversed(range(length)):
            continuing = ~result["terminal"][t]
            delta = result["rewards"][t] + bootstrap * continuing - result["values"][t]
            advantage = delta + .99 * continuing * advantage
            result["advantages"][t] = advantage
            bootstrap = result["values"][t]
        result["returns"] = result["values"] + result["advantages"]
        result["policy_mask"] = result["policy_mask"].reshape(-1, 128)
        for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "action_valid", "advantages", "returns"):
            result[name] = result[name].flatten()
        completed = self.completed[self.partial["completed_before"]:]
        self.last_stall_classes = dict(Counter(e["stall_class"] for e in completed if e["outcome"] == "stalled"))
        result["health"] = torch.tensor([len(completed), sum(e["floor"] for e in completed),
            sum(e["outcome"] == "stalled" for e in completed),
            max((self.floor_steps[s] for s in live), default=0)], dtype=torch.int64, device=device)
        self.partial = None
        return result

    @torch.no_grad()
    def evaluate(self, model, seeds):
        if self.evaluation_waiting is None:
            self.evaluation_waiting = deque(seeds)
            self.completed = []
            self.seeds = [None] * self.search.lanes
            self.pending = [deque() for _ in self.seeds]
        waiting = self.evaluation_waiting
        while waiting or any(s is not None for s in self.seeds):
            self.search.clock()
            free = [s for s in range(self.search.lanes) if self.seeds[s] is None]
            count = min(len(waiting), len(free))
            if count:
                self.reset(free[:count], [waiting.popleft() for _ in range(count)])
            slots = [s for s, seed in enumerate(self.seeds) if seed is not None]
            progress = self.search.progress()
            choices, _, _, _ = self.choose(model, slots, progress, training=False)
            completed_before = len(self.completed)
            self.advance(slots, choices, progress)
            if len(self.completed) // 64 != completed_before // 64:
                print(json.dumps(dict(event="evaluation_progress", rank=self.rank,
                    completed=len(self.completed), waiting=len(waiting), trials=self.search.trials)), flush=True)
        self.evaluation_waiting = None
        return self.completed
