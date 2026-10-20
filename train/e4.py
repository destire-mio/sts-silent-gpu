from collections import deque
from dataclasses import dataclass, field
import json
import time

import torch

from .model import FIELDS as views_fields, distribution, network_inputs, store_inputs, views
from .rollout import make_batch


FLOOR, ACT, PHASE, ROOM, SCREEN, KEYS, BATTLE_OVER, SMOKED, HP = range(9)
COMBAT, EVENT, MAP, ROOM_SCREEN, DEATH, VICTORY = 0, 8, 9, 10, 4, 3
COMBAT_PHASE, COMPLETE_PHASE = 1, 2
BOSS_ROOM, ELITE_ROOM, EVENT_ROOM, NEOW_ROOM = 2, 1, 6, 8
MAX_REWARD = 57 + 4 * 5 + 3 * 3 + 10 + 10 + 50
REACH = ("act1_boss", "act2_boss", "act3_first_boss", "act3_second_boss",
         "act3_three_keys", "act4", "shield_spear", "heart")


class AllocationEnded(Exception):
    pass


@dataclass
class RewardLedger:
    floors: set = field(default_factory=set)
    keys: set = field(default_factory=set)
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
        for key in range(3):
            if after[KEYS] & (1 << key) and key not in self.keys:
                self.keys.add(key)
                reward += 3
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
        self.batch = make_batch(lanes + scratch_slots, 2**63 - 1, device)
        self.arrays = views(self.batch, device)
        for name in ("entity_counts", "candidate_counts", "faults", "done", "truncated"):
            self.arrays[name] = torch.as_tensor(getattr(self.batch, name), device=device)
        self.progress_buffer = torch.empty(self.batch.size * 9, dtype=torch.int64, device=device)
        self.available = list(range(lanes, self.batch.size))
        self.inference_chunk = inference_chunk
        self.deadline = float("inf")
        self.searched_decisions = 0
        self.provenance = {}
        self.evidence_directory = None

    def clock(self):
        if time.monotonic() >= self.deadline:
            raise AllocationEnded()

    def progress(self):
        self.batch.progress_tensors(self.progress_buffer)
        return self.progress_buffer.view(-1, 9).cpu().tolist()

    def check(self, slots):
        faults = self.arrays["faults"].to(torch.int64)[slots].cpu().tolist()
        if any(faults) and self.evidence_directory is not None:
            evidence = [dict(slot=s, fault=f, seed=self.provenance[s][0],
                             candidates=self.provenance[s][1]) for s, f in zip(slots, faults) if f]
            (self.evidence_directory / f"fault-device-{self.device.index}.json").write_text(json.dumps(evidence))
        assert not any(faults), f"E4 engine fault: {list(zip(slots, faults))}"
        assert not bool(self.arrays["truncated"][slots].any()), "Unresolved search truncation"

    @torch.no_grad()
    def predict(self, model, slots):
        choices, values, logs = [], [], []
        for group in torch.tensor(slots, device=self.device).split(self.inference_chunk):
            inputs = network_inputs({k: self.arrays[k] for k in views_fields}, self.device, group)
            logits, value = model(inputs)
            policy, _ = distribution(logits, inputs["candidate_mask"])
            action = torch.multinomial(policy.probs, 1, generator=self.rng).flatten()
            choices.extend(action.cpu().tolist())
            values.extend(value.cpu().tolist())
            logs.extend(policy.log_prob(action).cpu().tolist())
        return choices, values, logs

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

    def step(self, slots, choices):
        indices = torch.tensor(slots, device=self.device, dtype=torch.int64)
        selected = torch.zeros(self.batch.size, dtype=torch.int32, device=self.device)
        active = torch.zeros(self.batch.size, dtype=torch.uint8, device=self.device)
        selected[indices] = torch.tensor(choices, device=self.device, dtype=torch.int32)
        active[indices] = 1
        self.batch.step_tensors(selected, active)
        for s, a in zip(slots, choices):
            self.provenance[s][1].append(a)
        self.check(slots)

    def clone(self, sources, targets):
        self.batch.clone_many(sources, targets)
        for s, t in zip(sources, targets):
            seed, path = self.provenance[s]
            self.provenance[t] = (seed, path.copy())

    def take(self, count):
        assert count <= len(self.available)
        slots = self.available[-count:]
        del self.available[-count:]
        return slots

    def combat(self, model, sources):
        selected_paths = {}
        capacity = len(self.available) // self.trials
        assert capacity > 0
        for begin in range(0, len(sources), capacity):
            parents = sources[begin:begin + capacity]
            slots = self.take(len(parents) * self.trials)
            try:
                self.clone([s for s in parents for _ in range(self.trials)], slots)
                paths = {s: [] for s in slots}
                live = slots[:]
                while live:
                    self.clock()
                    choices, _, _ = self.predict(model, live)
                    self.step(live, choices)
                    self.searched_decisions += len(live)
                    for s, a in zip(live, choices):
                        paths[s].append(a)
                    progress = self.progress()
                    live = [s for s in live if progress[s][PHASE] == COMBAT_PHASE
                            and progress[s][SCREEN] not in (DEATH, VICTORY)]
                scores = self.scores(model, slots, progress)
                winners = [max(slots[j * self.trials:(j + 1) * self.trials], key=scores.get)
                           for j in range(len(parents))]
                self.clone(winners, parents)
                selected_paths.update((parent, paths[winner]) for parent, winner in zip(parents, winners))
            finally:
                self.available.extend(slots)
        return selected_paths

    @staticmethod
    def event_exit(p):
        return (p[SCREEN] in (MAP, DEATH, VICTORY)
                or p[ROOM] not in (EVENT_ROOM, NEOW_ROOM, 11)
                or (p[SCREEN] == ROOM_SCREEN and p[PHASE] == COMPLETE_PHASE))

    def event(self, model, sources):
        selected_paths = {}
        for parent in sources:
            kinds = self.arrays["candidate_kinds"][parent].cpu().tolist()
            options = [i for i, kind in enumerate(kinds) if kind == 3]
            count = len(options)
            assert count
            slots = self.take(count)
            try:
                self.clone([parent] * count, slots)
                paths = {s: [a] for a, s in zip(options, slots)}
                self.step(slots, options)
                self.searched_decisions += count
                live = slots[:]
                while live:
                    self.clock()
                    progress = self.progress()
                    live = [s for s in live if not self.event_exit(progress[s])]
                    fighting = [s for s in live if progress[s][PHASE] == COMBAT_PHASE]
                    if fighting:
                        branches = self.combat(model, fighting)
                        for s, actions in branches.items():
                            paths[s].extend(actions)
                        continue
                    if live:
                        choices, _, _ = self.predict(model, live)
                        self.step(live, choices)
                        self.searched_decisions += len(live)
                        for s, a in zip(live, choices):
                            paths[s].append(a)
                progress = self.progress()
                scores = self.scores(model, slots, progress)
                winner = max(slots, key=scores.get)
                self.clone([winner], [parent])
                selected_paths[parent] = paths[winner]
            finally:
                self.available.extend(slots)
        return selected_paths

    def plan(self, model, sources, kind):
        backups = self.take(len(sources))
        self.clone(sources, backups)
        rng_before = self.rng.get_state()
        try:
            return self.combat(model, sources) if kind == "combat" else self.event(model, sources)
        except AllocationEnded:
            self.rng.set_state(rng_before)
            raise
        finally:
            self.clone(backups, sources)
            self.available.extend(backups)


class SegmentCollector:
    def __init__(self, search, seed_rng, *, rank=0):
        self.search, self.seed_rng, self.rank = search, seed_rng, rank
        self.seeds = [None] * search.lanes
        self.paths = [[] for _ in self.seeds]
        self.pending = [deque() for _ in self.seeds]
        self.rewards = [RewardLedger() for _ in self.seeds]
        self.completed = []
        self.decisions = 0
        self.partial = None
        self.evaluation_waiting = None

    def reset(self, slots, seeds):
        for s, seed in zip(slots, seeds):
            self.search.provenance[s] = (seed, [])
        self.search.batch.reset_to_many(slots, seeds, [[] for _ in slots])
        self.search.check(slots)
        for slot, seed in zip(slots, seeds):
            self.seeds[slot] = seed
            self.paths[slot] = []
            self.pending[slot].clear()
            self.rewards[slot] = RewardLedger()

    def fresh(self, slots):
        seeds = torch.randint(1000000000, 2000000000, (len(slots),),
                              generator=self.seed_rng).tolist()
        self.reset(slots, seeds)

    def state(self):
        return dict(seeds=self.seeds, paths=self.paths,
                    pending=[list(p) for p in self.pending], rewards=self.rewards,
                    seed_rng=self.seed_rng.get_state(), search_rng=self.search.rng.get_state(),
                    decisions=self.decisions, completed=self.completed, partial=self.partial,
                    evaluation_waiting=self.evaluation_waiting)

    def restore(self, state):
        for name in ("seeds", "paths", "rewards", "decisions", "completed", "partial", "evaluation_waiting"):
            setattr(self, name, state[name])
        if self.partial:
            for name, value in self.partial["result"].items():
                self.partial["result"][name] = value.to(self.search.device)
        self.pending = [deque(p) for p in state["pending"]]
        self.seed_rng.set_state(state["seed_rng"])
        self.search.rng.set_state(state["search_rng"])
        slots = [i for i, seed in enumerate(self.seeds) if seed is not None]
        if slots:
            for s in slots:
                self.search.provenance[s] = (self.seeds[s], self.paths[s].copy())
            self.search.batch.reset_to_many(slots, [self.seeds[i] for i in slots],
                                            [self.paths[i] for i in slots])
            self.search.check(slots)

    def choose(self, model, slots, progress):
        actions, values, logs = self.search.predict(model, slots)
        chosen = dict(zip(slots, actions))
        idle = [s for s in slots if not self.pending[s]]
        combat = [s for s in idle if progress[s][PHASE] == COMBAT_PHASE]
        events = [s for s in idle if progress[s][PHASE] != COMBAT_PHASE and progress[s][SCREEN] == EVENT
                  and int(self.search.arrays["candidate_kinds"][s, chosen[s]]) == 3]
        if combat:
            for s, path in self.search.plan(model, combat, "combat").items():
                self.pending[s].extend(path)
        if events:
            for s, path in self.search.plan(model, events, "event").items():
                self.pending[s].append(path[0])
        imitation = []
        for j, s in enumerate(slots):
            imitation.append(bool(self.pending[s]))
            if self.pending[s]:
                actions[j] = self.pending[s].popleft()
        return actions, values, logs, imitation

    def advance(self, slots, actions, progress):
        self.search.step(slots, actions)
        after = self.search.progress()
        reward, terminal = [], []
        for slot, action in zip(slots, actions):
            self.paths[slot].append(action)
            self.decisions += 1
            reward.append(self.rewards[slot].advance(progress[slot], after[slot]))
            end = after[slot][SCREEN] in (DEATH, VICTORY)
            terminal.append(end)
            if end:
                assert not self.pending[slot]
                self.completed.append(dict(seed=self.seeds[slot], candidates=self.paths[slot],
                    reach=self.rewards[slot].reach.copy(), floor=after[slot][FLOOR],
                    outcome="victory" if after[slot][SCREEN] == VICTORY else "death",
                    reward=self.rewards[slot].total / MAX_REWARD))
                self.seeds[slot] = None
        return reward, terminal

    @torch.no_grad()
    def collect(self, model, length=64):
        device, lanes = self.search.device, self.search.lanes
        slots = list(range(lanes))
        index = torch.arange(lanes, device=device)
        if self.partial is None:
            result = dict(data=torch.empty(64 * 1024 * 1024, dtype=torch.uint8, device=device),
                          offsets=torch.zeros(length * lanes + 1, dtype=torch.int64, device=device))
            for name in ("actions", "values", "logs", "rewards", "terminal", "imitation"):
                dtype = torch.int64 if name == "actions" else torch.bool if name in ("terminal", "imitation") else torch.float32
                result[name] = torch.zeros((length, lanes), device=device, dtype=dtype)
            self.partial = dict(result=result, next_step=0, length=length)
        assert self.partial["length"] == length
        result = self.partial["result"]
        for t in range(self.partial["next_step"], length):
            self.search.clock()
            fresh = [s for s in slots if self.seeds[s] is None]
            if fresh:
                self.fresh(fresh)
            progress = self.search.progress()
            actions, values, logs, imitation = self.choose(model, slots, progress)
            row = t * lanes
            store_inputs(self.search.batch, self.search.arrays, index, result["data"],
                         result["offsets"][row:row + lanes + 1])
            reward, terminal = self.advance(slots, actions, progress)
            for name, data in (("actions", actions), ("values", values), ("logs", logs),
                               ("rewards", reward), ("terminal", terminal), ("imitation", imitation)):
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
        for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "advantages", "returns"):
            result[name] = result[name].flatten()
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
            choices, _, _, _ = self.choose(model, slots, progress)
            completed_before = len(self.completed)
            self.advance(slots, choices, progress)
            if len(self.completed) // 64 != completed_before // 64:
                print(json.dumps(dict(event="evaluation_progress", rank=self.rank,
                    completed=len(self.completed), waiting=len(waiting), trials=self.search.trials)), flush=True)
        self.evaluation_waiting = None
        return self.completed
