"""Training-only starts from retained Act1 Boss, Act2, Act2 Boss and Act3 states."""
from collections import deque
from copy import deepcopy
import torch
from train.e4 import ACT, FLOOR, PHASE, ROOM, BOSS_ROOM, COMBAT_PHASE
from train.e4_stage12 import NoSLCollector

class MidstartCollector(NoSLCollector):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.pools = [[], [], [], []]
        self.starts = ['fresh'] * self.search.lanes
        self.captured = [[False, False, False, False] for _ in self.starts]
        self.randomizations = [[] for _ in self.starts]
        self.pending_randomization = [None] * self.search.lanes
        self.starting_rewards = [0] * self.search.lanes
        self.search.midstart_collector = self

    def reset(self, slots, seeds):
        super().reset(slots, seeds)
        for slot in slots:
            self.starts[slot] = 'fresh'
            self.captured[slot] = [False, False, False, False]
            self.randomizations[slot] = []
            self.pending_randomization[slot] = None
            self.starting_rewards[slot] = 0

    def retain(self, pool, slot):
        entries = self.pools[pool]
        position = len(entries) if len(entries) < 256 else int(torch.randint(256, (), generator=self.seed_rng))
        target = self.search.lanes + pool * 256 + position
        self.captured[slot][pool] = True
        self.search.clone([slot], [target])
        entry = self.pool_entry(slot, target)
        if position == len(entries):
            entries.append(entry)
        else:
            entries[position] = entry

    def pool_entry(self, slot, target):
        return dict(seed=self.seeds[slot], path=self.paths[slot].copy(),
            reward=deepcopy(self.rewards[slot]), floor_steps=self.floor_steps[slot],
            noncombat_floor_steps=self.noncombat_floor_steps[slot], tails=list(self.tails[slot]),
            pre_boss_deck_sizes=deepcopy(self.pre_boss_deck_sizes[slot]),
            captured=self.captured[slot].copy(), randomizations=deepcopy(self.randomizations[slot]),
            seen=deepcopy(self.search.seen[target]), skip_returns=deepcopy(self.search.skip_returns[target]),
            shop_skip_used=self.search.shop_skip_used[target], shop_opened=self.search.shop_opened[target],
            reward_history=deepcopy(self.search.reward_history[target]),
            map_returned=bool(self.search.map_returned[target]))

    def restart(self, pool, slot):
        position = int(torch.randint(len(self.pools[pool]), (), generator=self.seed_rng))
        entry = self.pools[pool][position]
        source = self.search.lanes + pool * 256 + position
        kind = ('act1_boss', 'act2', 'act2_boss', 'act3')[pool]
        self.start_from_entry(entry, source, slot, kind)

    def start_from_entry(self, entry, source, slot, kind):
        self.search.failed.pop(slot, None)
        self.search.clone([source], [slot])
        self.seeds[slot] = entry['seed']
        self.paths[slot] = entry['path'].copy()
        self.rewards[slot] = deepcopy(entry['reward'])
        self.floor_steps[slot] = entry['floor_steps']
        self.noncombat_floor_steps[slot] = entry['noncombat_floor_steps']
        self.tails[slot] = deque(entry['tails'], maxlen=10)
        self.pending[slot].clear()
        self.pre_boss_deck_sizes[slot] = deepcopy(entry['pre_boss_deck_sizes'])
        self.captured[slot] = entry['captured'].copy()
        self.randomizations[slot] = deepcopy(entry['randomizations'])
        self.pending_randomization[slot] = int(torch.randint(2**63 - 1, (), generator=self.seed_rng))
        self.starts[slot] = kind
        self.starting_rewards[slot] = self.rewards[slot].total
        self.round_games += 1

    def fresh(self, slots):
        if min(map(len, self.pools[:2])) < 32:
            return super().fresh(slots)
        choices = torch.randint(100, (len(slots),), generator=self.seed_rng).tolist()
        new = [slot for slot, choice in zip(slots, choices) if choice < 45]
        if new:
            super().fresh(new)
        for slot, choice in zip(slots, choices):
            if choice >= 45:
                pool = 0 if choice < 55 else 1 if choice < 75 else 2 if choice < 90 else 3
                if len(self.pools[pool]) < 32:
                    pool = 1
                self.restart(pool, slot)

    def choose(self, model, slots, progress):
        randomized = [s for s in slots if self.pending_randomization[s] is not None
                      and s not in self.search.failed]
        if randomized:
            seeds = [self.pending_randomization[s] for s in randomized]
            self.search.batch.determinize_slots(randomized, seeds, True)
            for slot, seed in zip(randomized, seeds):
                self.randomizations[slot].append((len(self.paths[slot]), seed, True))
                self.pending_randomization[slot] = None
                self.search.frame_cache.pop(slot, None)
                self.search.mask_cache.pop(slot, None)
            self.search.check(randomized)
        self.capture_starts(slots, progress)
        return super().choose(model, slots, progress)

    def capture_starts(self, slots, progress):
        for slot in slots:
            if slot in self.search.failed:
                continue
            p = progress[slot]
            if not self.captured[slot][0] and p[ACT] == 1 and p[ROOM] == BOSS_ROOM and p[PHASE] == COMBAT_PHASE:
                self.retain(0, slot)
            if not self.captured[slot][1] and p[ACT] == 2:
                self.retain(1, slot)
            if not self.captured[slot][2] and p[ACT] == 2 and p[ROOM] == BOSS_ROOM and p[PHASE] == COMBAT_PHASE:
                self.retain(2, slot)
            if not self.captured[slot][3] and p[ACT] == 3:
                self.retain(3, slot)

    def advance(self, slots, actions, progress):
        count = len(self.completed)
        result = super().advance(slots, actions, progress)
        # Completed lanes have already cleared their seed; terminal order follows slots.
        ended = [s for s, terminal in zip(slots, result[1]) if terminal]
        for episode, slot in zip(self.completed[count:], ended):
            episode['start_kind'] = self.starts[slot]
            episode['reward_since_start'] = (
                self.rewards[slot].total - self.starting_rewards[slot]) / self.rewards[slot].reward_budget
        return result

    def state(self):
        state = super().state()
        state['midstart'] = dict(pools=deepcopy(self.pools), starts=self.starts.copy(),
            captured=deepcopy(self.captured), randomizations=deepcopy(self.randomizations),
            pending_randomization=self.pending_randomization.copy(),
            starting_rewards=self.starting_rewards.copy())
        return state

    def replay(self, slots, entries):
        if not slots:
            return
        positions = [e['randomizations'][0][0] if e['randomizations'] else len(e['path']) for e in entries]
        self.search.batch.reset_to_many(slots, [e['seed'] for e in entries],
            [e['path'][:at] for e, at in zip(entries, positions)])
        events = [deque(e['randomizations']) for e in entries]
        while True:
            indices = [j for j, queue in enumerate(events) if queue and queue[0][0] == positions[j]]
            while indices:
                current = [(j, events[j].popleft()) for j in indices]
                for run in (False, True):
                    group = [(j, event) for j, event in current
                             if (event[2] if len(event) == 3 else False) == run]
                    if group:
                        self.search.batch.determinize_slots([slots[j] for j, _ in group],
                            [event[1] for _, event in group], run)
                indices = [j for j, queue in enumerate(events) if queue and queue[0][0] == positions[j]]
            active = [j for j, e in enumerate(entries) if positions[j] < len(e['path'])]
            if not active:
                break
            self.search.batch.step_slots([slots[j] for j in active], [entries[j]['path'][positions[j]] for j in active])
            for j in active:
                positions[j] += 1
        self.search.check(slots)

    def restore(self, state):
        # Pre-midstart checkpoints migrate with empty pools and their original live lanes intact.
        if 'midstart' not in state:
            super().restore(state)
            slots = [s for s, seed in enumerate(self.seeds) if seed is not None]
            for slot, p in self.search.progress_for(slots).items():
                self.captured[slot] = [p[ACT] > 1 or p[FLOOR] >= 16, p[ACT] >= 2,
                                       p[ACT] > 2 or (p[ACT] == 2 and p[ROOM] == BOSS_ROOM), p[ACT] >= 3]
            return
        saved = state['midstart']
        self.pools = deepcopy(saved['pools'])
        self.starts = saved['starts'].copy()
        self.captured = deepcopy(saved['captured'])
        old_pool_count = len(self.pools)
        for _ in range(old_pool_count, 4):
            self.pools.append([])
            self.captured = [row + [False] for row in self.captured]
            for pool in self.pools[:old_pool_count]:
                for entry in pool:
                    entry['captured'].append(False)
        self.randomizations = deepcopy(saved['randomizations'])
        self.pending_randomization = saved['pending_randomization'].copy()
        self.starting_rewards = saved['starting_rewards'].copy()
        original_paths = state['paths']
        prefixes = [path[:events[0][0]] if events else path for path, events in zip(original_paths, self.randomizations)]
        super().restore(dict(state, paths=prefixes))
        self.paths = deepcopy(original_paths)
        slots = [s for s, seed in enumerate(self.seeds) if seed is not None]
        entries = [dict(seed=self.seeds[s], path=self.paths[s], randomizations=self.randomizations[s]) for s in slots]
        changed = [(s, e) for s, e in zip(slots, entries) if e['randomizations']]
        self.replay([s for s, _ in changed], [e for _, e in changed])
        for slot in slots:
            self.search.provenance[slot] = (self.seeds[slot], self.paths[slot].copy())
        pool_slots, pool_entries = [], []
        for pool, entries in enumerate(self.pools):
            for position, entry in enumerate(entries):
                slot = self.search.lanes + pool * 256 + position
                pool_slots.append(slot); pool_entries.append(entry)
                self.restore_pool_metadata(slot, entry)
        self.replay(pool_slots, pool_entries)
        if old_pool_count < 4:
            for slot, p in self.search.progress_for(slots).items():
                if old_pool_count == 2:
                    self.captured[slot][2] = p[ACT] > 2 or (p[ACT] == 2 and p[ROOM] == BOSS_ROOM)
                self.captured[slot][3] = p[ACT] >= 3

    def restore_pool_metadata(self, slot, entry):
        self.search.provenance[slot] = (entry['seed'], entry['path'].copy())
        self.search.seen[slot] = deepcopy(entry['seen'])
        self.search.skip_returns[slot] = deepcopy(entry['skip_returns'])
        self.search.shop_skip_used[slot] = entry['shop_skip_used']
        self.search.shop_opened[slot] = entry['shop_opened']
        self.search.reward_history[slot] = deepcopy(entry['reward_history'])
        self.search.map_returned[slot] = entry['map_returned']
