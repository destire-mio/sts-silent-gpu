"""Combat expert iteration: separate lanes, preview-only labels, no PPO/value rows."""
from collections import deque
from copy import deepcopy
import math
import time
import torch
import torch.distributed as dist
from train.e4 import AllocationEnded, PHASE, COMBAT_PHASE
from train.e4_stage12 import NoSLSearch, collector_snapshot, restore_collector
from train.e4_midstart import MidstartCollector
from train.e4_lookahead import CombatLookahead, LookaheadSearch
from train.model import packed_inputs

BETA = .5
MAX_LANES = 32


class ExpertBudgetEnded(Exception):
    pass


def next_lane_count(lanes, extra_seconds, base_seconds):
    """Use measured collection+CE time; retain headroom for changing combat cost."""
    if base_seconds <= 0 or extra_seconds <= 0:
        return max(1, min(MAX_LANES, lanes))
    return max(1, min(MAX_LANES, math.floor(lanes * .20 * base_seconds / extra_seconds)))


def ce_scalar(logits, legal, actions, world, total_samples, minibatches):
    """One epoch's minibatch-average gradient is beta times the global label mean."""
    masked = logits.float().masked_fill(~legal, -torch.inf)
    ce = torch.nn.functional.cross_entropy(masked, actions, reduction='none')
    return ce.sum() * (BETA * world * minibatches / max(1, total_samples)), ce


class ExpertSearch(LookaheadSearch):
    def __init__(self, device, seed):
        NoSLSearch.__init__(self, MAX_LANES, device, seed,
                            storage_slots=MAX_LANES * 32, greedy=True, probe_slots=128)
        self.lookahead = CombatLookahead(self, seed, depth='combat')
        self.preview_steps = self.mainline_decisions = self.preview_decisions = self.overrides = 0
        self.labels = {}
        self.soft_deadline = float('inf')

    def clock(self):
        super().clock()  # Original allocation deadline and yield file still take precedence.
        if time.monotonic() >= self.soft_deadline:
            raise ExpertBudgetEnded()

    def predict(self, model, slots):
        actions, values, logs = NoSLSearch.predict(self, model, slots)
        progress = self.progress_for(slots)
        positions = [i for i, s in enumerate(slots)
                     if s not in self.failed and progress[s][PHASE] == COMBAT_PHASE]
        self.labels = {}
        available = self.available.copy()
        try:
            if positions:
                chosen, reports = self.lookahead.select(model, [slots[i] for i in positions])
                for i, action, report in zip(positions, chosen, reports):
                    actions[i] = action
                    if report['preview_steps'] > 0:
                        self.labels[slots[i]] = dict(action=action,
                            agreement=action == report['candidates'][0],
                            preview_steps=report['preview_steps'])
        finally:
            # Interrupted previews never consume a mainline step or leak scratch slots.
            self.available = available
        return actions, values, logs


class ExpertCollector(MidstartCollector):
    """Read PPO pool entries without modifying their native slots, RNG, or metadata."""
    def __init__(self, *args, source_collector, **kwargs):
        super().__init__(*args, **kwargs)
        self.source_collector = source_collector

    def retain(self, pool, slot):
        self.captured[slot][pool] = True

    def fresh(self, slots):
        pools = self.source_collector.pools
        if min(map(len, pools[:2])) < 32:
            return super(MidstartCollector, self).fresh(slots)
        choices = torch.randint(100, (len(slots),), generator=self.seed_rng).tolist()
        new = [s for s, choice in zip(slots, choices) if choice < 45]
        if new:
            super(MidstartCollector, self).fresh(new)
        for slot, choice in zip(slots, choices):
            if choice >= 45:
                pool = 0 if choice < 55 else 1 if choice < 75 else 2 if choice < 90 else 3
                self.restart(pool if len(pools[pool]) >= 32 else 1, slot)

    def restart(self, pool, slot):
        entries = self.source_collector.pools[pool]
        entry = deepcopy(entries[int(torch.randint(len(entries), (), generator=self.seed_rng))])
        self.search.failed.pop(slot, None)
        self.search.provenance[slot] = (entry['seed'], entry['path'].copy())
        self.replay([slot], [entry])
        self.search.seen[slot] = entry['seen']
        self.search.skip_returns[slot] = entry['skip_returns']
        self.search.shop_skip_used[slot] = entry['shop_skip_used']
        self.search.shop_opened[slot] = entry['shop_opened']
        self.search.reward_history[slot] = entry['reward_history']
        self.search.map_returned[slot] = entry['map_returned']
        self.search.frame_cache.pop(slot, None)
        self.search.mask_cache.pop(slot, None)
        self.search.mainline_progress.pop(slot, None)
        self.seeds[slot], self.paths[slot] = entry['seed'], entry['path'].copy()
        self.rewards[slot] = entry['reward']
        self.floor_steps[slot] = entry['floor_steps']
        self.noncombat_floor_steps[slot] = entry['noncombat_floor_steps']
        self.tails[slot] = deque(entry['tails'], maxlen=10)
        self.pending[slot].clear()
        self.pre_boss_deck_sizes[slot] = entry['pre_boss_deck_sizes']
        self.captured[slot] = entry['captured']
        self.randomizations[slot] = entry['randomizations']
        self.pending_randomization[slot] = int(torch.randint(2**63 - 1, (), generator=self.seed_rng))
        self.starts[slot] = ('act1_boss', 'act2', 'act2_boss', 'act3')[pool]
        self.starting_rewards[slot] = self.rewards[slot].total
        self.round_games += 1


class ExpertIteration:
    def __init__(self, source_collector, output, deadline, rank, saved=None):
        device = source_collector.search.device
        self.device = device
        self.search = ExpertSearch(device, 20261380 + rank)
        self.search.deadline, self.search.evidence_directory = deadline, output
        self.collector = ExpertCollector(self.search,
            torch.Generator().manual_seed(20262380 + rank), rank=rank,
            source_collector=source_collector)
        self.lanes = 1
        self.base_seconds = None
        self.round = None
        self.setup_seconds = 0.
        if saved is not None:
            restore_collector(self.collector, saved['collector'])
            self.search.lookahead.rng.set_state(saved['preview_rng'].cpu())
            self.lanes, self.base_seconds = saved['lanes'], saved['base_seconds']
            self.round = saved['round']
            self.setup_seconds = saved.get('setup_seconds', 0.)
            if self.round:
                for row in self.round['records']:
                    for key in ('data', 'mask'):
                        row[key] = row[key].to(device)

    def state(self):
        from train.e4_stage6.training_worker import move_tensors
        return dict(collector=collector_snapshot(self.collector),
            preview_rng=self.search.lookahead.rng.get_state(), lanes=self.lanes,
            base_seconds=self.base_seconds,
            round=move_tensors(self.round, torch.device('cpu')), setup_seconds=self.setup_seconds)

    @torch.no_grad()
    def collect(self, policy, update, ppo_collection_seconds):
        if self.round is None:
            self.round = dict(update=update, ticks=0, records=[], seconds=0.,
                              agreement=0, preview_steps=0, done=False, lanes=self.lanes)
            self.collector.completed.clear()
        current = self.round
        assert current['update'] == update
        if current['done']:
            return
        baseline = self.base_seconds if self.base_seconds is not None else ppo_collection_seconds
        remaining = max(0., .20 * max(0., baseline) - current['seconds'])
        if remaining == 0:
            current['done'] = True
            return
        started = time.monotonic()
        self.search.soft_deadline = started + remaining
        slots = list(range(current['lanes']))
        policy.eval()
        try:
            while current['ticks'] < 64:
                self.search.clock()
                fresh = [s for s in slots if self.collector.seeds[s] is None]
                if fresh:
                    self.collector.fresh(fresh)
                progress = self.search.progress_for(slots)
                actions, _, _, _ = self.collector.choose(policy, slots, progress)
                selected = [s for s in slots if s in self.search.labels]
                rows = []
                if selected:
                    index = torch.tensor(selected, dtype=torch.int64, device=self.device)
                    lengths = (4 + 24 * self.search.arrays['entity_counts'][index].long()
                               + 7 * self.search.arrays['candidate_counts'][index].long())
                    offsets = torch.cat((lengths.new_zeros(1), lengths.cumsum(0)))
                    data = torch.empty(int(offsets[-1]), dtype=torch.uint8, device=self.device)
                    self.search.batch.store_tensors(index, data, offsets)
                    for j, slot in enumerate(selected):
                        rows.append((slot, dict(data=data[int(offsets[j]):int(offsets[j+1])].clone(),
                            mask=self.search.last_policy_mask[slots.index(slot)].clone(),
                            **self.search.labels[slot])))
                self.collector.advance(slots, actions, progress)
                for slot, row in rows:
                    if slot not in self.collector.stalled:
                        current['records'].append(row)
                        current['agreement'] += int(row['agreement'])
                        current['preview_steps'] += row['preview_steps']
                current['ticks'] += 1
            current['done'] = True
        except ExpertBudgetEnded:
            current['done'] = True
        finally:
            current['seconds'] += time.monotonic() - started
            self.search.soft_deadline = float('inf')

    def fragment(self):
        rows = self.round['records']
        offsets = torch.tensor([0] + [len(r['data']) for r in rows],
                               dtype=torch.int64, device=self.device).cumsum(0)
        return dict(data=torch.cat([r['data'] for r in rows]) if rows else
                    torch.empty(0, dtype=torch.uint8, device=self.device),
            offsets=offsets, actions=torch.tensor([r['action'] for r in rows],
                dtype=torch.int64, device=self.device),
            policy_mask=torch.stack([r['mask'] for r in rows]) if rows else
                torch.empty((0,128), dtype=torch.bool, device=self.device))

    def finish(self, cursor, round_seconds, log):
        extra = cursor.get('expert_ce_seconds', 0.) + self.round['seconds'] + self.setup_seconds
        values = torch.tensor([len(self.round['records']), self.round['agreement'],
            len(self.collector.completed), sum(e['floor'] for e in self.collector.completed),
            *cursor.get('expert_ce_stats', [0., 0.])], dtype=torch.float64, device=self.device)
        dist.all_reduce(values)
        times = torch.tensor([extra, self.round['seconds'],
            cursor.get('expert_ce_seconds',0.)], dtype=torch.float64, device=self.device)
        dist.all_reduce(times, op=dist.ReduceOp.MAX)
        extra = float(times[0])
        base = max(.001, round_seconds - extra)
        used_lanes = self.round['lanes']
        self.lanes = next_lane_count(used_lanes, extra, base)
        self.base_seconds = base
        row = dict(expert_samples=int(values[0]), expert_beta=BETA,
            expert_ce=float(values[4]/values[5]) if float(values[5]) else None,
            expert_argmax_agreement=float(values[1]/values[0]) if float(values[0]) else None,
            expert_completed=int(values[2]),
            expert_mean_floor=float(values[3]/values[2]) if float(values[2]) else None,
            expert_lanes=used_lanes, expert_next_lanes=self.lanes,
            expert_extra_seconds=extra, expert_setup_seconds=self.setup_seconds,
            expert_collection_seconds=float(times[1]),
            expert_ce_seconds=float(times[2]), expert_base_seconds=base,
            expert_overhead_ratio=extra/base, expert_target_overhead=.30,
            expert_overhead_target_met=extra/base <= .30,
            expert_calibration='measured_natural_GPU_round')
        self.round = None
        self.setup_seconds = 0.
        self.collector.completed.clear()
        return row


def attach_expert(cursor, fragment, world):
    count = torch.tensor(len(fragment['actions']), device=cursor['order'].device, dtype=torch.int64)
    dist.all_reduce(count)
    cursor['expert'] = fragment
    cursor['expert_total'] = int(count)
    cursor['expert_minibatches'] = math.ceil(sum(cursor['counts']) / 32768)
    cursor['expert_ce_stats'] = [0., 0.]
    cursor['expert_ce_seconds'] = 0.


def expert_backward(model, scaler, cursor, rank, world, microbatch):
    """Accumulate CE in the same Adam step before the existing PPO backward/sync."""
    if not cursor.get('expert_total'):
        return
    fragment = cursor['expert']
    minibatches = cursor['expert_minibatches']
    batch = cursor['start'] // 32768
    ids = torch.arange(len(fragment['actions']), device=cursor['order'].device)[batch::minibatches]
    started = time.monotonic()
    with model.no_sync():
        for subset in ids.split(microbatch):
            if not len(subset):
                continue
            inputs = packed_inputs(fragment['data'], fragment['offsets'], subset, cursor['order'].device)
            inputs['candidate_mask'] &= fragment['policy_mask'][subset, :inputs['candidate_mask'].shape[1]]
            logits, values = model(inputs)
            scalar, ce = ce_scalar(logits, inputs['candidate_mask'], fragment['actions'][subset],
                                    world, cursor['expert_total'], minibatches)
            cursor['finite'] &= torch.isfinite(ce).all()
            scaler.scale(scalar + values.sum() * 0.).backward()
            cursor['expert_ce_stats'][0] += float(ce.detach().double().sum())
            cursor['expert_ce_stats'][1] += len(subset)
    if cursor['order'].device.type == 'cuda':
        torch.cuda.synchronize(cursor['order'].device)
    cursor['expert_ce_seconds'] += time.monotonic() - started
