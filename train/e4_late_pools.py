"""Train-only late roots and natural Stage12 midstarts."""
from collections import Counter
import gzip
import hashlib
import json
import random
import time

import torch

from train.e4 import ACT, BOSS_ROOM, COMBAT_PHASE, ELITE_ROOM, FLOOR, HP, KEYS, MAP, PHASE, ROOM, SCREEN
from train.e4_midstart import MIDSTART_NAMES, MidstartCollector
from train.e4_rewards import RewardHistory
from train.e4_stage12_arena import ArenaCollector
from train.rollout import make_batch, make_root_cache


LATE_KINDS = ("act3_boss", "act4_entry", "heart_start", "heart_mid")
LATE_CAPACITY = 256
ORIGIN_CAPACITY = {"e2": LATE_CAPACITY, "self": LATE_CAPACITY // 2}
LATE_SLOTS = len(LATE_KINDS) * sum(ORIGIN_CAPACITY.values())
HEART_TURNS = {"heart_turn2": 2, "heart_turn3": 3, "heart_turn5": 5}


def read_e2_late_roots(path):
    groups = {kind: [] for kind in LATE_KINDS}
    seen = set()
    with gzip.open(path, "rt", encoding="utf-8") as source:
        for line in source:
            row = json.loads(line)
            if row["partition"] != "train":
                continue
            stage, root = row["stage"], row["root"]
            kind = "heart_mid" if stage in HEART_TURNS else stage
            if kind not in groups:
                continue
            identity = (root["seed"], stage)
            assert identity not in seen and 1_000_000_000 <= root["seed"] <= 1_999_999_999
            assert "provenance" not in row and root["candidates"]
            seen.add(identity)
            groups[kind].append(row)
    return groups


def late_boundary(progress, turn):
    act, room, phase = progress[ACT], progress[ROOM], progress[PHASE]
    if act == 3 and room == BOSS_ROOM and phase == COMBAT_PHASE and turn == 1:
        return "act3_boss", "act3_boss"
    if act == 4:
        if progress[SCREEN] == MAP and progress[FLOOR] == 51:
            return "act4_entry", "act4_entry"
        if room == ELITE_ROOM and phase == COMBAT_PHASE and turn == 1:
            return "act4_entry", "shield_spear"
        if room == BOSS_ROOM and phase == COMBAT_PHASE:
            if turn == 1:
                return "heart_start", "heart_start"
            if turn in HEART_TURNS.values():
                return "heart_mid", f"heart_turn{turn}"
    return None


def late_bank_counts(external, generated):
    return min(external, LATE_CAPACITY - generated), generated


def replay_matches(row, progress, turn):
    match = late_boundary(progress, turn)
    expected = row["stage"]
    keys = sum(int(held) << i for i, held in enumerate(row["keys"]))
    return (match is not None and match[1] == expected and progress[FLOOR] == row["floor"]
            and progress[HP] == row["hp"] and progress[KEYS] == keys and turn == row["turn"])


class LateMidstartCollector(MidstartCollector):
    def __init__(self, search, seed_rng, *, late_fraction, late_offset, **kwargs):
        super().__init__(search, seed_rng, **kwargs)
        self.late_fraction = late_fraction
        self.late_bases = {}
        slot = search.lanes + late_offset
        for kind in LATE_KINDS:
            for origin, capacity in ORIGIN_CAPACITY.items():
                self.late_bases[kind, origin] = slot
                slot += capacity
        self.late_banks = {key: [] for key in self.late_bases}
        self.late_positions = dict.fromkeys(LATE_KINDS, 0)
        self.late_marks = [set() for _ in self.seeds]
        self.late_initial_reach = [None for _ in self.seeds]
        self.late_started = Counter()
        self.late_initialization = None
        self.late_turns = torch.empty(search.size, dtype=torch.int64, device=search.device)

    def _turns(self, slots):
        self.search.batch.combat_turns_tensors(self.late_turns)
        return self.late_turns[slots].tolist()

    def _reset_e2_prefixes(self, slots, seeds, paths):
        # The released E2 roots were collected with the default boss discovery mask.
        # Keep ordinary PPO lanes on their own profile when transferring these roots.
        device = self.search.device
        capacity = min(len(slots), LATE_CAPACITY)
        source = make_batch(capacity, 2**63 - 1, device, bosses_seen=0)
        cache = make_root_cache(capacity, device)
        indices = torch.arange(capacity, device=device)
        for start in range(0, len(slots), capacity):
            stop = min(start + capacity, len(slots))
            roots = indices[:stop - start]
            source.reset_to_many(list(range(stop - start)), seeds[start:stop], paths[start:stop])
            source.cache_roots_tensors(cache, roots, roots)
            targets = torch.tensor(slots[start:stop], dtype=torch.int64, device=device)
            self.search.batch.reset_cached_tensors(cache, targets, roots)

    def _replay_failures(self, slots):
        arrays = self.search.arrays
        faults = arrays["faults"].long()[slots].tolist()
        truncated = arrays["truncated"][slots].tolist()
        return {slot: fault for slot, fault, cut in zip(slots, faults, truncated) if fault or cut}

    def _check_e2_replay(self, slots):
        failures = self._replay_failures(slots)
        if failures:
            raise RuntimeError(f"Could not restore E2 checkpoint slots: {failures}")

    def seed_external(self, path):
        if self.late_initialization is not None:
            return self.late_initialization
        started = time.monotonic()
        source = read_e2_late_roots(path)
        order = random.Random(198)
        results = {}
        for kind, rows in source.items():
            order.shuffle(rows)
            base = self.late_bases[kind, "e2"]
            bank = self.late_banks[kind, "e2"]
            attempted, rejected = 0, 0
            faults = Counter()
            while attempted < len(rows) and len(bank) < LATE_CAPACITY:
                chunk = rows[attempted:attempted + LATE_CAPACITY - len(bank)]
                attempted += len(chunk)
                slots = list(range(base + len(bank), base + len(bank) + len(chunk)))
                entries = [self._external_entry(row) for row in chunk]
                search = self.search
                self._reset_e2_prefixes(slots, [e["seed"] for e in entries], [e["path"] for e in entries])
                failures = self._replay_failures(slots)
                faults.update(failures.values())
                progress = search.progress_for(slots)
                turns = self._turns(slots)
                live = [slot for slot in slots if slot not in failures]
                keys = dict(zip(live, search.loop_keys(live))) if live else {}
                for slot, entry, row, turn in zip(slots, entries, chunk, turns):
                    if slot not in keys or not replay_matches(row, progress[slot], turn):
                        rejected += 1
                        continue
                    entry["seen"].add(keys[slot])
                    self.restore_pool_metadata(slot, entry)
                    target = base + len(bank)
                    if target != slot:
                        search.clone([slot], [target])
                    bank.append(entry)
            results[kind] = dict(available=len(rows), attempted=attempted, loaded=len(bank), rejected=rejected,
                                 native_faults=dict(faults))
        with open(path, "rb") as file:
            digest = hashlib.sha256(file.read()).hexdigest()
        self.late_initialization = dict(pools=results, source_sha256=digest, seconds=time.monotonic()-started)
        return self.late_initialization

    def _external_entry(self, row):
        root = row["root"]
        reward = self.reward_ledger_type()
        reward.reach.update(act1_boss=True, act2_boss=True)
        if row["stage"] != "act3_boss":
            reward.reach.update(act3_first_boss=True, act3_second_boss=True, act3_three_keys=True, act4=True)
        return dict(seed=root["seed"], path=root["candidates"].copy(), reward=reward,
                    floor_steps=0, noncombat_floor_steps=0, tails=[], pre_boss_deck_sizes=[],
                    captured=[True] * 4, randomizations=[], seen=set(), skip_returns=set(),
                    shop_skip_used=False, shop_opened=False, reward_history=RewardHistory(), map_returned=False)

    def reset(self, slots, seeds):
        super().reset(slots, seeds)
        for slot in slots:
            self.late_marks[slot].clear()
            self.late_initial_reach[slot] = None
        self.late_started["fresh"] += len(slots)

    def start_from_entry(self, entry, source, slot, kind):
        super().start_from_entry(entry, source, slot, kind)
        self.late_initial_reach[slot] = entry["reward"].reach.copy()
        if not kind.startswith("arena_"):
            self.late_started[kind] += 1

    def fresh(self, slots):
        if not self.late_fraction or not any(self.late_banks.values()):
            return super().fresh(slots)
        ordinary = []
        draws = torch.rand(len(slots), generator=self.seed_rng).tolist()
        for slot, draw in zip(slots, draws):
            if draw >= self.late_fraction:
                ordinary.append(slot)
                continue
            kind = LATE_KINDS[min(len(LATE_KINDS)-1, int(draw * len(LATE_KINDS) / self.late_fraction))]
            external, generated = late_bank_counts(len(self.late_banks[kind, "e2"]),
                                                    len(self.late_banks[kind, "self"]))
            if not external + generated:
                ordinary.append(slot)
                continue
            i = int(torch.randint(external + generated, (), generator=self.seed_rng))
            origin, index = ("e2", i) if i < external else ("self", i - external)
            key = kind, origin
            entry = self.late_banks[key][index]
            label = f"late_{kind}_{origin}"
            self.start_from_entry(entry, self.late_bases[key] + index, slot, label)
        if ordinary:
            super().fresh(ordinary)

    def capture_starts(self, slots, progress):
        super().capture_starts(slots, progress)
        candidates = [slot for slot in slots if slot not in self.search.failed
                      and self.starts[slot] == "fresh" and progress[slot][ACT] >= 3
                      and (progress[slot][ROOM] == BOSS_ROOM
                           or progress[slot][ACT] == 4 and (progress[slot][ROOM] == ELITE_ROOM
                                                          or progress[slot][SCREEN] == MAP))]
        if not candidates:
            return
        for slot, turn in zip(candidates, self._turns(candidates)):
            boundary = late_boundary(progress[slot], turn)
            if boundary is None:
                continue
            kind, marker = boundary
            if marker in self.late_marks[slot]:
                continue
            self.late_marks[slot].add(marker)
            key = kind, "self"
            bank = self.late_banks[key]
            pos = self.late_positions[kind]
            target = self.late_bases[key] + pos
            self.search.clone([slot], [target])
            entry = self.pool_entry(slot, target)
            if pos == len(bank):
                bank.append(entry)
            else:
                bank[pos] = entry
            self.late_positions[kind] = (pos + 1) % ORIGIN_CAPACITY["self"]

    def advance(self, slots, actions, progress):
        count = len(self.completed)
        rewards, terminal = super().advance(slots, actions, progress)
        ended = (slot for slot, done in zip(slots, terminal) if done)
        for episode, slot in zip(self.completed[count:], ended):
            before = self.late_initial_reach[slot]
            if self.starts[slot].startswith("late_") and self.starts[slot].endswith("_e2"):
                episode["bosses_seen"] = 0
            episode["since_start_milestones"] = {name: achieved and (before is None or not before[name])
                                                 for name, achieved in episode["reach"].items()}
        return rewards, terminal

    def late_round_metrics(self):
        counts = ("act1_boss", "act2_boss", "act3_first_boss", "act3_second_boss", "act4", "shield_spear", "heart")
        names = ["fresh", *MIDSTART_NAMES, *(f"late_{kind}_{origin}" for kind, origin in self.late_banks)]
        result = {kind: dict(started=self.late_started[kind], completed=0, **dict.fromkeys(counts, 0))
                  for kind in names}
        for episode in self.completed:
            kind = episode["start_kind"]
            if episode.get("outcome") != "stalled" and kind in result:
                row = result[kind]
                row["completed"] += 1
                milestones = episode.get("since_start_milestones", episode["reach"])
                for name in counts:
                    row[name] += milestones[name]
        self.late_started.clear()
        return result

    def state(self):
        state = super().state()
        state["late_pools"] = dict(banks=self.late_banks, positions=self.late_positions, marks=self.late_marks,
                                   initial_reach=self.late_initial_reach, started=dict(self.late_started),
                                   initialization=self.late_initialization)
        return state

    def restore(self, state):
        saved = state.get("late_pools")
        if saved is None:
            super().restore(state)
            return
        e2_live = {slot for slot, (seed, start) in enumerate(zip(state["seeds"], state["midstart"]["starts"]))
                   if seed is not None and start.startswith("late_") and start.endswith("_e2")}
        ordinary = [None if slot in e2_live else seed for slot, seed in enumerate(state["seeds"])]
        super().restore(dict(state, seeds=ordinary))
        self.seeds = state["seeds"]
        self.late_banks = saved["banks"]
        self.late_positions = saved["positions"]
        self.late_marks = saved["marks"]
        self.late_initial_reach = saved["initial_reach"]
        self.late_started = Counter(saved["started"])
        self.late_initialization = saved["initialization"]
        e2_slots, e2_entries = [], []
        for slot in sorted(e2_live):
            self.search.provenance[slot] = (self.seeds[slot], self.paths[slot].copy())
            e2_slots.append(slot)
            e2_entries.append(dict(seed=self.seeds[slot], path=self.paths[slot], randomizations=self.randomizations[slot]))
        slots, entries = [], []
        for key, bank in self.late_banks.items():
            for position, entry in enumerate(bank):
                slot = self.late_bases[key] + position
                self.restore_pool_metadata(slot, entry)
                if key[1] == "e2":
                    e2_slots.append(slot)
                    e2_entries.append(entry)
                else:
                    slots.append(slot)
                    entries.append(entry)
        self.replay(e2_slots, e2_entries, reset_prefixes=self._reset_e2_prefixes, check=self._check_e2_replay)
        self.replay(slots, entries)


class LateArenaCollector(ArenaCollector, LateMidstartCollector):
    pass
