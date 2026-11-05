"""Combat expert iteration: separate lanes, preview-only labels, no PPO/value rows."""
from collections import deque
from copy import deepcopy
import math
import gc
import json
import time
import torch
import torch.distributed as dist
from train import OBSERVATION_VERSION
from train.e4 import AllocationEnded, PHASE, COMBAT_PHASE
from train.e4_stage12 import NoSLSearch, collector_snapshot, restore_collector, restart_collector_observation
from train.e4_midstart import MidstartCollector
from train.e4_lookahead import CombatLookahead, LookaheadSearch
from train.model import packed_inputs

CE_GRAD_NORM_CAP = .25
MAX_LANES = 512


def memory_limited_lanes(requested, ppo_peak, device_total, per_lane):
    """Allocated PPO bytes plus the estimated complete expert-lane footprint."""
    limit = max(0, device_total - 1536 * 1024**2)
    allowed = max(1, (limit - ppo_peak) // max(1, per_lane))
    return max(1, min(MAX_LANES, requested, allowed)), limit


def gpu_memory(device):
    if device.type != 'cuda':
        return None
    _, total = torch.cuda.mem_get_info(device)
    return dict(total=total, peak=torch.cuda.max_memory_allocated(device),
        allocated=torch.cuda.memory_allocated(device),
        allocated_peak=torch.cuda.max_memory_allocated(device))


def enlarged_collector_state(saved, blank):
    """Extend only lane-indexed state, preserving every existing live game and RNG."""
    state = deepcopy(saved)
    for key in ('seeds','paths','floor_steps','noncombat_floor_steps','tails',
                'map_returned','pending','rewards','pre_boss_deck_sizes','reward_history'):
        state[key] = deepcopy(saved[key]) + deepcopy(blank[key][len(saved[key]):])
    for key in ('starts','captured','randomizations','pending_randomization','starting_rewards'):
        state['midstart'][key] = (deepcopy(saved['midstart'][key])
                                 + deepcopy(blank['midstart'][key][len(saved['midstart'][key]):]))
    return state


class ExpertBudgetEnded(Exception):
    pass


def next_lane_count(lanes, extra_seconds, base_seconds, *, ticks, samples_per_second, previous):
    if extra_seconds / base_seconds > .30:
        requested = lanes // 2
    elif ticks == 64 or previous is None or samples_per_second >= 1.2 * previous['samples_per_second']:
        requested = lanes * 2
    elif samples_per_second < previous['samples_per_second']:
        requested = previous['lanes']
    else:
        requested = lanes
    return max(1, min(MAX_LANES, requested))


def ce_scalar(logits, legal, actions, world, total_samples, minibatches):
    """Global label mean before the independent gradient-norm cap."""
    masked = logits.float().masked_fill(~legal, -torch.inf)
    ce = torch.nn.functional.cross_entropy(masked, actions, reduction='none')
    return ce.sum() * (world * minibatches / max(1, total_samples)), ce


def override_label(report):
    return (report['preview_steps'] > 0
            and report['selected'] != report['candidates'][0]
            and report['paired_mean'] > report['paired_SE'])


class ExpertSearch(LookaheadSearch):
    def __init__(self, device, seed, lanes=1):
        NoSLSearch.__init__(self, lanes, device, seed,
                            storage_slots=lanes * 32, greedy=True, probe_slots=128)
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
                    if override_label(report):
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
        self.source_collector, self.output, self.deadline, self.rank = source_collector, output, deadline, rank
        self.capacity = len(saved['collector']['seeds']) if saved is not None else 1
        memory_before = gpu_memory(device)
        self.search = ExpertSearch(device, 20261380 + rank, self.capacity)
        self.search.deadline, self.search.evidence_directory = deadline, output
        self.collector = ExpertCollector(self.search,
            torch.Generator().manual_seed(20262380 + rank), rank=rank,
            source_collector=source_collector)
        memory_after = gpu_memory(device)
        self.memory = {}
        if memory_before is not None:
            native = max(1,memory_after['allocated']-memory_before['allocated'])
            self.memory = dict(previous_peak=memory_before['peak'],
                native_per_lane=math.ceil(native*33/(33*self.capacity+128)),
                working_per_lane=math.ceil(max(64*1024**2,memory_before['allocated_peak']-memory_before['allocated'])*64/2048))
        self.lanes = 1
        self.base_seconds = None
        self.round = None
        self.setup_seconds = 0.
        self.throughput = None
        if saved is not None:
            previous_observation = saved.get('observation_version', 13)
            assert previous_observation in (13, OBSERVATION_VERSION)
            migrating = previous_observation != OBSERVATION_VERSION
            collector_state = (restart_collector_observation(saved['collector'], collector_snapshot(self.collector))
                               if migrating else saved['collector'])
            restore_collector(self.collector, collector_state)
            self.search.lookahead.rng.set_state(saved['preview_rng'].cpu())
            self.lanes, self.base_seconds = saved['lanes'], saved['base_seconds']
            self.round = None if migrating else saved['round']
            self.setup_seconds = 0. if migrating else saved.get('setup_seconds', 0.)
            self.memory = saved.get('memory',self.memory)
            self.throughput = None if migrating else saved.get('throughput')
            if self.round:
                for row in self.round['records']:
                    for key in ('data', 'mask'):
                        row[key] = row[key].to(device)

    def begin_ppo_round(self):
        self.memory['previous_ppo_allocated_peak'] = self.memory.get('ppo_allocated_peak', 0)
        self.memory['ppo_allocated_peak'] = 0
        if self.device.type == 'cuda':
            torch.cuda.reset_peak_memory_stats(self.device)

    def record_ppo_peak(self):
        measured = gpu_memory(self.device)
        if measured is not None:
            self.memory['ppo_allocated_peak'] = max(self.memory.get('ppo_allocated_peak', 0), measured['allocated_peak'])

    def prepare_lanes(self):
        requested = self.lanes
        measured = gpu_memory(self.device)
        safe = requested
        if measured is not None:
            # Collection precedes this round's learner; include the last measured
            # PPO learner peak as well as this round's PPO collection peak.
            ppo_peak = max(self.memory.get('ppo_allocated_peak', 0),
                           self.memory.get('previous_ppo_allocated_peak', 0))
            recent = self.memory.get('measured_lane_history', [])
            if not recent and 'measured_per_lane' in self.memory:
                recent = [self.memory['measured_per_lane']]
            per_lane = (math.ceil(1.5 * max(recent)) if recent
                        else self.memory['native_per_lane'] + self.memory['working_per_lane'])
            safe, limit = memory_limited_lanes(requested, ppo_peak, measured['total'], per_lane)
            self.memory['last_limit'] = limit
            self.memory['projection_base'] = ppo_peak
            self.memory['lane_cost'] = per_lane
        if dist.is_initialized():
            common = torch.tensor(safe,dtype=torch.int64,device=self.device)
            dist.all_reduce(common,op=dist.ReduceOp.MIN)
            safe = int(common)
        if safe < requested:
            print(json.dumps(dict(event='expert_lane_memory_limit',rank=self.rank,
                requested_lanes=requested,retained_lanes=safe,
                previous_peak_bytes=self.memory.get('previous_peak'),
                growth_limit_bytes=self.memory.get('last_limit'),
                projected_allocated_bytes=self.memory['projection_base'] + requested*self.memory['lane_cost'],
                reason='allocated PPO peak plus expert lanes exceeds total minus 1.5 GiB')),flush=True)
        self.lanes = safe
        if safe <= self.capacity:
            return
        started = time.monotonic()
        saved = self.state()
        source = self.source_collector
        del self.collector, self.search
        gc.collect()
        if self.device.type == 'cuda':
            torch.cuda.empty_cache()
        try:
            self._restore_capacity(safe, saved, source, growing=True)
        except torch.cuda.OutOfMemoryError:
            # Recover the previously fitting geometry before reporting the OOM
            # to every rank. The PPO collector and optimizer are untouched.
            self.__dict__.pop('collector', None)
            self.__dict__.pop('search', None)
            gc.collect()
            torch.cuda.empty_cache()
            self._restore_capacity(self.capacity, saved, source, growing=False)
            raise
        self.capacity = safe
        self.setup_seconds += time.monotonic()-started

    def _restore_capacity(self, capacity, saved, source, *, growing):
        self.search = ExpertSearch(self.device,20261380+self.rank,capacity)
        self.search.deadline, self.search.evidence_directory = self.deadline,self.output
        self.collector = ExpertCollector(self.search,
            torch.Generator().manual_seed(20262380+self.rank),rank=self.rank,
            source_collector=source)
        state = (enlarged_collector_state(saved['collector'],collector_snapshot(self.collector))
                 if growing else saved['collector'])
        restore_collector(self.collector,state)
        self.search.lookahead.rng.set_state(saved['preview_rng'].cpu())

    def discard_oom(self, stage):
        current = self.round
        used = current['lanes'] if current is not None else self.lanes
        self.lanes = max(1, used // 2)
        if current is not None:
            current.update(records=[], agreement=0, preview_steps=0, done=True,
                           oom_stage=stage)
        self.search.labels = {}
        self.search.frame_cache.clear()
        self.search.mask_cache.clear()
        self.collector.completed.clear()
        self.throughput = None
        if self.device.type == 'cuda':
            torch.cuda.empty_cache()
        print(json.dumps(dict(event='expert_oom_recovered', rank=self.rank,
            stage=stage, discarded_round_samples=True, previous_lanes=used,
            next_lanes=self.lanes, PPO_continues=True)), flush=True)

    def collect_and_fragment(self, policy, update, ppo_collection_seconds):
        self.record_ppo_peak()
        before = gpu_memory(self.device)
        if before is not None:
            torch.cuda.reset_peak_memory_stats(self.device)
        flag = torch.zeros((),dtype=torch.int64,device=self.device)
        fragment = None
        try:
            self.collect(policy, update, ppo_collection_seconds)
            fragment = self.fragment()
        except torch.cuda.OutOfMemoryError:
            flag.fill_(1)
        # AllocationEnded must still reach the caller's original yield protocol;
        # all ranks participate in that protocol before this OOM collective.
        except AllocationEnded:
            flag.fill_(2)
        dist.all_reduce(flag,op=dist.ReduceOp.MAX)
        if int(flag) == 2:
            raise AllocationEnded()
        if bool(flag):
            fragment = None
            if self.round is None:
                self.round = dict(update=update,ticks=0,records=[],seconds=0.,
                                  agreement=0,preview_steps=0,done=True,lanes=self.lanes)
            self.discard_oom('collection')
            fragment = self.fragment()
        after = gpu_memory(self.device)
        if before is not None:
            increment = max(0,after['allocated_peak']-before['allocated'])
            recent = self.memory.get('measured_lane_history', [])
            if not recent and 'measured_per_lane' in self.memory:
                recent = [self.memory['measured_per_lane']]
            measured = math.ceil(increment / self.round['lanes'])
            self.memory['measured_per_lane'] = measured
            self.memory['measured_lane_history'] = (recent + [measured])[-3:]
            self.memory['collection_allocated_peak'] = after['allocated_peak']
            torch.cuda.reset_peak_memory_stats(self.device)
        return fragment

    def state(self):
        from train.e4_stage6.training_worker import move_tensors
        return dict(collector=collector_snapshot(self.collector),
            preview_rng=self.search.lookahead.rng.get_state(), lanes=self.lanes,
            base_seconds=self.base_seconds,
            round=move_tensors(self.round, torch.device('cpu')), setup_seconds=self.setup_seconds,
            capacity=self.capacity, memory=deepcopy(self.memory),
            observation_version=OBSERVATION_VERSION, throughput=deepcopy(self.throughput))

    @torch.no_grad()
    def collect(self, policy, update, ppo_collection_seconds):
        if self.round is None:
            self.prepare_lanes()
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
            cursor.get('expert_ce_seconds',0.), -self.round['ticks']], dtype=torch.float64, device=self.device)
        dist.all_reduce(times, op=dist.ReduceOp.MAX)
        extra = float(times[0])
        base = max(.001, round_seconds - extra)
        used_lanes = self.round['lanes']
        self.memory['last_active'] = used_lanes
        ticks = int(-times[3])
        samples_per_second = float(values[0]) / max(.001, float(times[1]))
        oom_stage = self.round.get('oom_stage')
        if not oom_stage:
            self.lanes = next_lane_count(used_lanes, extra, base, ticks=ticks,
                samples_per_second=samples_per_second, previous=self.throughput)
            self.throughput = dict(lanes=used_lanes, samples_per_second=samples_per_second)
        self.base_seconds = base
        measured_memory = gpu_memory(self.device)
        # previous_peak is the pre-expert pure-PPO anchor, preserved by checkpoints.
        gradients = cursor.get('expert_gradient_stats', [0., 0., 0., 0.])
        row = dict(expert_samples=int(values[0]), expert_beta=None,
            expert_version=2, expert_collection=True,
            expert_ce_grad_norm=gradients[0]/gradients[3] if gradients[3] else None,
            expert_ppo_grad_norm=gradients[1]/gradients[3] if gradients[3] else None,
            expert_ce_scale=gradients[2]/gradients[3] if gradients[3] else None,
            expert_ce_gradient_cap=CE_GRAD_NORM_CAP,
            expert_ce=float(values[4]/values[5]) if float(values[5]) else None,
            expert_argmax_agreement=float(values[1]/values[0]) if float(values[0]) else None,
            expert_completed=int(values[2]),
            expert_mean_floor=float(values[3]/values[2]) if float(values[2]) else None,
            expert_lanes=used_lanes, expert_next_lanes=self.lanes,
            expert_ticks=ticks, expert_samples_per_second=samples_per_second,
            expert_storage_lanes=self.capacity, expert_max_lanes=MAX_LANES,
            expert_gpu_peak_bytes=max(measured_memory['allocated_peak'],
                self.memory.get('collection_allocated_peak',0),
                cursor.get('expert_ce_allocated_peak',0)) if measured_memory is not None else None,
            expert_memory_anchor_peak_bytes=self.memory.get('previous_peak'),
            expert_memory_growth_limit_bytes=self.memory.get('last_limit'),
            expert_ppo_allocated_peak_bytes=self.memory.get('projection_base'),
            expert_projected_allocated_bytes=self.memory.get('projection_base',0)+used_lanes*self.memory.get('lane_cost',0),
            expert_collection_allocated_peak_bytes=self.memory.get('collection_allocated_peak'),
            expert_measured_bytes_per_lane=self.memory.get('measured_per_lane'),
            expert_oom_stage=oom_stage,
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


def _expert_backward(model, scaler, cursor, rank, world, microbatch):
    """Compute local CE without DDP sync; keep it separate from PPO."""
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
            # Uneven CE batches must not enter DDP forward's bucket-rebuild collectives.
            logits, values = model.module(inputs)
            scalar, ce = ce_scalar(logits, inputs['candidate_mask'], fragment['actions'][subset],
                                    world, cursor['expert_total'], minibatches)
            cursor['finite'] &= torch.isfinite(ce).all()
            scaler.scale(scalar + values.sum() * 0.).backward()
            cursor['expert_ce_stats'][0] += float(ce.detach().double().sum())
            cursor['expert_ce_stats'][1] += len(subset)
    if cursor['order'].device.type == 'cuda':
        torch.cuda.synchronize(cursor['order'].device)
    cursor['expert_ce_seconds'] += time.monotonic() - started


def expert_backward(model, scaler, cursor, rank, world, microbatch, oom_callback=None):
    if not cursor.get('expert_total'):
        return
    device = cursor['order'].device
    flag = torch.zeros((),dtype=torch.int64,device=device)
    statistics_before = cursor['expert_ce_stats'].copy()
    finite_before = cursor['finite'].clone()
    cpu_rng = torch.get_rng_state()
    cuda_rng = torch.cuda.get_rng_state(device) if device.type == 'cuda' else None
    started = time.monotonic()
    seconds_before = cursor.get('expert_ce_seconds',0.)
    gradients = None
    try:
        _expert_backward(model, scaler, cursor, rank, world, microbatch)
        gradients = torch.cat([p.grad.detach().reshape(-1) if p.grad is not None
                               else torch.zeros_like(p).reshape(-1)
                               for p in model.parameters()])
    except torch.cuda.OutOfMemoryError:
        flag.fill_(1)
    cursor['expert_ce_seconds'] = seconds_before + time.monotonic()-started
    if device.type == 'cuda':
        cursor['expert_ce_allocated_peak'] = max(cursor.get('expert_ce_allocated_peak',0),
                                                torch.cuda.max_memory_allocated(device))
    dist.all_reduce(flag,op=dist.ReduceOp.MAX)
    if bool(flag):
        # No Adam step has occurred for this minibatch. Discard every rank's
        # partial CE gradients; the original PPO backward/clip/Adam follows.
        model.zero_grad(set_to_none=True)
        gradients = None
        cursor['expert_ce_stats'] = statistics_before
        cursor['finite'] = finite_before
        cursor['expert_total'] = 0
        cursor['expert'] = None
        cursor['expert_oom_stage'] = 'CE'
        torch.set_rng_state(cpu_rng)
        if cuda_rng is not None:
            torch.cuda.set_rng_state(cuda_rng,device)
        if oom_callback is not None:
            oom_callback('CE')
        if device.type == 'cuda':
            torch.cuda.empty_cache()
    if device.type == 'cuda':
        torch.cuda.reset_peak_memory_stats(device)
    model.zero_grad(set_to_none=True)
    return gradients


def add_capped_expert_gradients(model, gradients, scaler, cursor, world):
    """PPO is already DDP-averaged; average CE and cap before AMP unscale."""
    parameters = list(model.parameters())
    ppo = torch.linalg.vector_norm(torch.stack([p.grad.norm() for p in parameters]))
    ce = ppo.new_zeros(())
    factor = ppo.new_ones(())
    if gradients is not None:
        dist.all_reduce(gradients)
        gradients.div_(world)
        ce = gradients.norm()
        if bool(ce > 0):
            factor = torch.minimum(factor, CE_GRAD_NORM_CAP * ppo / ce)
        gradients.mul_(factor)
        offset = 0
        for p in parameters:
            end = offset + p.numel()
            p.grad.add_(gradients[offset:end].view_as(p))
            offset = end
    stats = cursor.setdefault('expert_gradient_stats', [0., 0., 0., 0.])
    stats[0] += float(ce) / scaler.get_scale()
    stats[1] += float(ppo) / scaler.get_scale()
    stats[2] += float(factor)
    stats[3] += 1
