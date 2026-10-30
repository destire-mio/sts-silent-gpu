"""Stage12: from-scratch CLS policy and no-search PPO."""
from contextlib import nullcontext, contextmanager
from collections import Counter
from copy import deepcopy
import math
import json
import time
from pathlib import Path
import random

import numpy as np
import torch
from torch import nn
import torch.distributed as dist

from train.e4 import (RoomSearch, SegmentCollector, PHASE, COMBAT_PHASE, FLOOR, SCREEN, SCREENS,
    ACT, ROOM, BOSS_ROOM, DEATH, VICTORY, MAX_REWARD, ACTION_KINDS)
from train.model import Policy, ModelConfig, network_inputs, distribution, packed_inputs, record_lengths
from train.e4_stage6.training_worker import move_tensors


class CLSPolicy(Policy):
    def __init__(self):
        super().__init__(ModelConfig(width=256, layers=4, heads=4, feedforward=1024))
        self.cls = nn.Parameter(torch.zeros(1, 1, 256))
        nn.init.normal_(self.cls, std=.02)
        self.encoder_fp16 = False

    def encode(self, observation):
        mask = observation["entity_mask"]
        device = mask.device
        with torch.autocast(device_type=device.type, dtype=torch.float16,
                            enabled=self.encoder_fp16 and device.type == "cuda"):
            values = observation["values"].transpose(1, 2)
            entities = self.numbers(values.sign() * values.abs().log1p())
            for row, embedding in enumerate(self.tags):
                entities = entities + embedding(observation["tags"][:, row])
            for row, embedding in enumerate(self.ids):
                entities = entities + embedding(observation["ids"][:, row])
            entities = self.input_norm(entities)
            entities = torch.cat((self.cls.expand(len(entities), -1, -1), entities), dim=1)
            mask = torch.cat((torch.ones(len(mask), 1, dtype=torch.bool, device=device), mask), dim=1)
            for layer in self.encoder:
                entities = layer(entities, src_key_padding_mask=~mask)
        return entities[:, 1:].float(), entities[:, 0].float()


class StageTimes:
    def __init__(self):
        self.seconds = dict.fromkeys(('inference', 'engine_step', 'masks_frames', 'repeat_probes', 'reward', 'learner'), 0.)
        self.stack = []

    @contextmanager
    def measure(self, name):
        frame = [time.perf_counter(), 0.]
        self.stack.append(frame)
        try:
            yield
        finally:
            elapsed = time.perf_counter() - frame[0]
            self.stack.pop()
            self.seconds[name] += elapsed - frame[1]
            if self.stack:
                self.stack[-1][1] += elapsed


class NoSLSearch(RoomSearch):
    def __init__(self, lanes, device, seed):
        super().__init__(lanes, 1, device, torch.Generator(device=device).manual_seed(seed),
                         scratch_slots=0, inference_chunk=lanes, bosses_seen=511, probe_slots=4096)
        self.entropy_sum = torch.zeros((), dtype=torch.float64, device=device)
        self.entropy_count = 0
        self.times = StageTimes()
        self.mainline_progress = {}
        self.last_policy_mask = None
        self.failed = {}
        self.fault_counts = Counter()

    def record_fault(self, slot, reason, extra_action=None):
        seed, path = self.provenance_record(slot)
        parent = self.provenance[slot][1] if slot >= self.lanes else slot
        if parent in self.failed:
            return
        self.failed[parent] = reason
        self.fault_counts[reason] += 1
        record = dict(event='episode_fault', seed=seed,
                      candidates=path + [extra_action] if extra_action is not None else path,
                      reason=reason, slot=parent, probe_slot=slot if slot >= self.lanes else None)
        print(json.dumps(record), flush=True)
        if self.evidence_directory is not None:
            with (self.evidence_directory / f'episode-faults-{self.device.index}.jsonl').open('a') as stream:
                stream.write(json.dumps(record) + '\n')

    def check_rows(self, slots, faults, truncated, combat_context):
        failed = set()
        for slot, fault, cut in zip(slots, faults, truncated):
            if fault or cut:
                self.record_fault(slot, f'engine_fault:{fault}' if fault else 'search_truncation')
                failed.add(slot)
        return failed

    def clock(self):
        if time.monotonic() - self.last_heartbeat >= 60:
            if self.evidence_directory is not None and (self.evidence_directory / 'stop-requested').exists():
                from train.e4 import AllocationEnded
                raise AllocationEnded()
            self.actual_lanes = [dict(lane=s, seed=self.provenance[s][0], steps=len(self.provenance[s][1]),
                                      floor=p[FLOOR], screen=SCREENS[p[SCREEN]])
                                 for s, p in self.mainline_progress.items()]
        return super().clock()

    def frames(self, slots, progress):
        with self.times.measure('masks_frames'):
            return super().frames(slots, progress)

    def probe_pairs(self, pairs, progress, frames, repeated):
        with self.times.measure('repeat_probes'):
            return super().probe_pairs(pairs, progress, frames, repeated)

    @torch.no_grad()
    def policy_masks(self, slots):
        with self.times.measure('masks_frames'):
            if any(s not in self.mainline_progress for s in slots):
                self.mainline_progress.update(self.progress_for(slots))
            packed = self.arrays['candidate_mask'][slots]
            result = ((packed[..., None].long() >> torch.arange(8, device=self.device)) & 1).flatten(1).bool()
            positions = [i for i, s in enumerate(slots) if s not in self.failed and self.mainline_progress[s][PHASE] != COMBAT_PHASE]
            if positions:
                result[positions] = super().policy_masks([slots[i] for i in positions])
            return result

    def step(self, slots, choices, combat_context=None):
        with self.times.measure('engine_step'):
            active = [(s, a) for s, a in zip(slots, choices) if s not in self.failed]
            self.last_step_progress = {}
            result = super().step([s for s, _ in active], [a for _, a in active], combat_context) if active else set()
            self.mainline_progress.clear()
            return result

    @torch.no_grad()
    def predict(self, model, slots):
        self.last_policy_mask = self.policy_masks(slots)
        choices, values, logs = [0] * len(slots), [0.] * len(slots), [0.] * len(slots)
        positions = [i for i, slot in enumerate(slots) if slot not in self.failed]
        if positions:
            with self.times.measure('inference'):
                index = torch.tensor([slots[i] for i in positions], dtype=torch.int64, device=self.device)
                inputs = network_inputs(self.arrays, self.device, index)
                inputs['candidate_mask'] &= self.last_policy_mask[positions, :inputs['candidate_mask'].shape[1]]
                logits, value = model(inputs)
                policy, _ = distribution(logits.float(), inputs['candidate_mask'])
                action = torch.multinomial(policy.probs, 1, generator=self.rng).flatten()
                rows = torch.stack((action.double(), value.double(), policy.log_prob(action).double()), dim=1).cpu().tolist()
                self.entropy_sum += policy.entropy().double().sum()
                self.entropy_count += len(positions)
            for i, row in zip(positions, rows):
                choices[i], values[i], logs[i] = int(row[0]), row[1], row[2]
        return choices, values, logs



class NoSLCollector(SegmentCollector):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.round_games = 0

    def reset(self, slots, seeds):
        for slot in slots:
            self.search.failed.pop(slot, None)
        self.round_games += len(slots)
        return super().reset(slots, seeds)

    def ready(self, slots, progress, last_step):
        return slots

    def choose(self, model, slots, progress):
        self.stalled = set()
        self.search.mainline_progress = dict(progress)
        return (*self.search.predict(model, slots), [False] * len(slots))

    def advance(self, slots, actions, progress):
        with self.search.times.measure('reward'):
            active = [(s, a) for s, a in zip(slots, actions) if s not in self.search.failed]
            if active:
                kinds = self.search.arrays['candidate_kinds'][[s for s, _ in active], [a for _, a in active]].cpu().tolist()
                for (slot, _), kind in zip(active, kinds):
                    self.tails[slot].append((SCREENS[progress[slot][SCREEN]], ACTION_KINDS[kind]))
                self.search.step([s for s, _ in active], [a for _, a in active])
            after = dict(progress)
            after.update(self.search.last_step_progress if active else {})
            rewards, terminal = [], []
            for slot, action in zip(slots, actions):
                failed = slot in self.search.failed
                if not failed:
                    self.paths[slot].append(action)
                    self.decisions += 1
                    same_floor = progress[slot][FLOOR] == after[slot][FLOOR]
                    self.floor_steps[slot] = self.floor_steps[slot] + 1 if same_floor else 0
                    self.noncombat_floor_steps[slot] = (self.noncombat_floor_steps[slot]
                        + int(progress[slot][PHASE] != COMBAT_PHASE)) if same_floor else 0
                    if (after[slot][ROOM] == BOSS_ROOM and after[slot][PHASE] == COMBAT_PHASE
                            and (progress[slot][ROOM] != BOSS_ROOM or progress[slot][PHASE] != COMBAT_PHASE or not same_floor)):
                        self.pre_boss_deck_sizes[slot].append(dict(seed=self.seeds[slot], act=after[slot][ACT],
                            floor=after[slot][FLOOR], deck_size=self.search.frames([slot], after)[slot]['deck_size']))
                limited = self.floor_steps[slot] > 3000 or self.noncombat_floor_steps[slot] > 200
                stalled = failed or limited
                reward = 0.
                if not stalled:
                    try:
                        reward = self.rewards[slot].advance(progress[slot], after[slot])
                    except AssertionError as error:
                        self.search.record_fault(slot, 'reward_ledger: ' + str(error))
                        stalled = True
                end = stalled or after[slot][SCREEN] in (DEATH, VICTORY)
                rewards.append(0. if stalled else reward)
                terminal.append(end)
                if stalled:
                    self.stalled.add(slot)
                if end:
                    faulty = slot in self.search.failed
                    frame = self.search.frame_cache.get(slot, {}) if faulty else self.search.frames([slot], after)[slot]
                    self.completed.append(dict(seed=self.seeds[slot], bosses_seen=self.search.bosses_seen,
                        candidates=self.paths[slot], reach=self.rewards[slot].reach.copy(),
                        floor=progress[slot][FLOOR] if faulty else after[slot][FLOOR],
                        stall_class=self.search.failed.get(slot, 'floor_step_limit' if limited else None),
                        noncombat=dict(counts=dict(self.search.reward_history[slot].counts)),
                        final_deck_size=frame.get('deck_size', 0),
                        pre_boss_deck_sizes=deepcopy(self.pre_boss_deck_sizes[slot]),
                        outcome='stalled' if stalled else 'victory' if after[slot][SCREEN] == VICTORY else 'death',
                        reward=self.rewards[slot].total / MAX_REWARD))
                    self.seeds[slot] = None
            return rewards, terminal

    @torch.no_grad()
    def collect(self, model, length=64):
        device, lanes = self.search.device, self.search.lanes
        slots = list(range(lanes))
        if self.partial is None:
            capacity = 10240
            result = dict(data=torch.empty(length * lanes * capacity, dtype=torch.uint8, device=device),
                          offsets=torch.zeros(length * lanes + 1, dtype=torch.int64, device=device),
                          policy_mask=torch.empty((length, lanes, 128), dtype=torch.bool, device=device))
            for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "action_valid", "combat"):
                dtype = torch.int64 if name == "actions" else torch.bool if name in ("terminal", "imitation", "action_valid", "combat") else torch.float32
                result[name] = torch.zeros((length, lanes), dtype=dtype, device=device)
            self.partial = dict(result=result, next_step=0, next_row=0, length=length,
                                completed_before=len(self.completed))
        result = self.partial["result"]
        for tick in range(self.partial["next_step"], length):
            self.search.clock()
            fresh = [s for s in slots if self.seeds[s] is None]
            if fresh:
                self.fresh(fresh)
            progress = self.search.progress_for(slots)
            actions, values, logs, imitation = self.choose(model, slots, progress)
            result["policy_mask"][tick] = self.search.last_policy_mask
            row = tick * lanes
            index = torch.tensor(slots, dtype=torch.int64, device=device)
            lengths = (4 + 24 * self.search.arrays["entity_counts"][index].long()
                       + 7 * self.search.arrays["candidate_counts"][index].long())
            result["offsets"][row + 1:row + lanes + 1] = result["offsets"][row] + lengths.cumsum(0)
            self.search.batch.store_tensors(index, result["data"], result["offsets"][row:row + lanes + 1])
            reward, terminal = self.advance(slots, actions, progress)
            for name, value in (("actions", actions), ("values", values), ("logs", logs), ("rewards", reward),
                                ("terminal", terminal), ("imitation", imitation),
                                ("action_valid", [s not in self.stalled for s in slots]),
                                ("combat", [progress[s][PHASE] == COMBAT_PHASE for s in slots])):
                result[name][tick] = torch.as_tensor(value, device=device, dtype=result[name].dtype)
            self.partial["next_step"] = tick + 1
            self.partial["next_row"] = (tick + 1) * lanes
        live = [s for s in slots if self.seeds[s] is not None]
        bootstrap = torch.zeros(lanes, device=device)
        if live:
            scores = self.search.scores(model, live, self.search.progress_for(live))
            bootstrap[live] = torch.tensor([scores[s] for s in live], device=device)
        advantages = torch.zeros_like(result["values"])
        carry = torch.zeros(lanes, device=device)
        for tick in reversed(range(length)):
            continuing = ~result["terminal"][tick]
            delta = result["rewards"][tick] + bootstrap * continuing - result["values"][tick]
            carry = delta + .995 * continuing * carry
            advantages[tick] = carry
            bootstrap = result["values"][tick]
        result["returns"] = result["values"] + advantages
        result["advantages"] = advantages
        result["policy_mask"] = result["policy_mask"].flatten(0, 1)
        for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "action_valid", "combat", "advantages", "returns"):
            result[name] = result[name].flatten()
        result["data"] = result["data"][:int(result["offsets"][-1])].clone()
        self.partial = None
        return result


def new_cursor(fragment, rng, world):
    device = fragment["actions"].device
    indices = fragment["action_valid"].nonzero().flatten()
    count = torch.tensor(len(indices), device=device, dtype=torch.int64)
    counts = [torch.empty_like(count) for _ in range(world)]
    dist.all_gather(counts, count)
    counts = [int(c) for c in counts]
    adv = fragment["advantages"][indices].double()
    moments = torch.stack((adv.sum(), adv.square().sum(), adv.new_tensor(len(adv))))
    dist.all_reduce(moments)
    mean = moments[0] / moments[2].clamp_min(1)
    std = (moments[1] / moments[2].clamp_min(1) - mean.square()).clamp_min(0).sqrt()
    fragment["advantages"] = ((fragment["advantages"] - mean) / (std + 1e-8)).float()
    return dict(fragment=fragment, indices=indices, counts=counts, epoch=0, start=0,
                order=torch.randperm(sum(counts), generator=rng, device=device),
                totals=torch.zeros(8, dtype=torch.float64, device=device),
                finite=torch.ones((), dtype=torch.bool, device=device), optimizer_steps=0,
                optimizer_attempts=0, amp_skipped_steps=0)


def local_minibatch(cursor, rank):
    ids = cursor["order"][cursor["start"]:cursor["start"] + 32768]
    offset = sum(cursor["counts"][:rank])
    owned = ids[(ids >= offset) & (ids < offset + cursor["counts"][rank])] - offset
    selected = cursor["indices"][owned]
    f = cursor["fragment"]
    if len(selected):
        entities, candidates = record_lengths(f["data"], f["offsets"], selected)
        selected = selected[torch.argsort((entities // 16) * 129 + candidates, stable=True)]
    return selected, len(ids)


@torch.no_grad()
def precision_probe(model, cursor, rank):
    selected, _ = local_minibatch(cursor, rank)
    f = cursor["fragment"]
    model.encoder_fp16 = True
    totals = torch.zeros(3, dtype=torch.float64, device=cursor["order"].device)
    for indices in selected.split(1024):
        inputs = packed_inputs(f["data"], f["offsets"], indices, indices.device)
        inputs["candidate_mask"] &= f["policy_mask"][indices, :inputs["candidate_mask"].shape[1]]
        logits, _ = model(inputs)
        policy, _ = distribution(logits, inputs["candidate_mask"])
        ratio = policy.log_prob(f["actions"][indices]) - f["logs"][indices]
        totals += torch.stack((ratio.abs().double().sum(), ((ratio.exp() - 1).abs() > .2).double().sum(),
                               ratio.new_tensor(len(indices), dtype=torch.float64)))
    dist.all_reduce(totals)
    steps = torch.tensor([cursor["optimizer_steps"], -cursor["optimizer_attempts"],
                          -cursor["amp_skipped_steps"], scaler.get_scale()], device=totals.device, dtype=torch.float64)
    dist.all_reduce(steps, op=dist.ReduceOp.MIN)
    applied, attempts, skipped, scale = steps.tolist()
    values = totals.tolist()
    mean, clipped = values[0] / max(1, values[2]), values[1] / max(1, values[2])
    model.encoder_fp16 = math.isfinite(mean) and mean <= .005 and clipped <= .01
    return dict(mean_abs_log_ratio=mean, clip_fraction=clipped,
                precision="encoder_fp16_heads_fp32" if model.encoder_fp16 else "fp32")


def advance_minibatch(model, optimizer, scaler, cursor, rng, rank, world, microbatch=1024):
    f = cursor["fragment"]
    selected, global_count = local_minibatch(cursor, rank)
    chunks = list(selected.split(microbatch))
    count = torch.tensor(len(chunks), device=cursor["order"].device)
    dist.all_reduce(count, op=dist.ReduceOp.MAX)
    optimizer.zero_grad(set_to_none=True)
    steps = int(count)
    for i in range(steps):
        ids = chunks[i] if i < len(chunks) else selected[:0]
        sync = model.no_sync() if i + 1 < steps else nullcontext()
        with sync:
            if len(ids):
                inputs = packed_inputs(f["data"], f["offsets"], ids, ids.device)
                inputs["candidate_mask"] &= f["policy_mask"][ids, :inputs["candidate_mask"].shape[1]]
                logits, value = model(inputs)
                policy, normalized_entropy = distribution(logits.float(), inputs["candidate_mask"])
                log_ratio = policy.log_prob(f["actions"][ids]) - f["logs"][ids]
                ratio = log_ratio.exp()
                actor = -torch.minimum(ratio * f["advantages"][ids],
                                       ratio.clamp(.8, 1.2) * f["advantages"][ids])
                value_loss = (value - f["returns"][ids]).square()
                loss = actor + .5 * value_loss - .01 * normalized_entropy
                scalar = loss.sum() * world / max(1, global_count)
                cursor["finite"] &= torch.isfinite(loss).all()
                cursor["totals"] += torch.stack((actor.detach().double().sum(), value_loss.detach().double().sum(),
                    policy.entropy().detach().double().sum(),
                    (ratio - 1 - log_ratio).detach().double().sum(),
                    ((ratio - 1).abs() > .2).double().sum(), loss.detach().double().sum(),
                    loss.new_tensor(len(ids), dtype=torch.float64), normalized_entropy.detach().double().sum()))
            else:
                from train.ppo import empty_inputs
                logits, value = model(empty_inputs(cursor["order"].device))
                scalar = (logits.sum() + value.sum()) * 0.
            scaler.scale(scalar).backward()
    scaler.unscale_(optimizer)
    norm = torch.nn.utils.clip_grad_norm_(model.parameters(), .5, error_if_nonfinite=False)
    scale_before = scaler.get_scale()
    scaler.step(optimizer)
    scaler.update()
    scale_after = scaler.get_scale()
    skipped = scale_after < scale_before
    cursor["optimizer_attempts"] += 1
    cursor["optimizer_steps"] += int(not skipped)
    cursor["amp_skipped_steps"] += int(skipped)
    if not bool(torch.isfinite(norm)):
        print(json.dumps(dict(event="nonfinite_gradient", rank=rank,
            gradient_norm=float(norm), scale_before=scale_before, scale_after=scale_after,
            optimizer_step_skipped=skipped, optimizer_attempt=cursor["optimizer_attempts"])), flush=True)
    cursor["start"] += 32768
    if cursor["start"] >= sum(cursor["counts"]):
        cursor["epoch"] += 1
        cursor["start"] = 0
        if cursor["epoch"] < 2:
            cursor["order"] = torch.randperm(sum(cursor["counts"]), generator=rng, device=cursor["order"].device)


def choose_microbatch(model, cursor):
    device = cursor['order'].device
    f = cursor['fragment']
    ids = cursor['indices']
    entities, _ = record_lengths(f['data'], f['offsets'], ids)
    ids = ids[torch.argsort(entities, descending=True)[:1024]]
    failed = torch.zeros((), dtype=torch.int64, device=device)
    try:
        with model.no_sync():
            inputs = packed_inputs(f['data'], f['offsets'], ids, device)
            logits, value = model(inputs)
            (logits.masked_fill(~inputs['candidate_mask'], 0).mean() + value.mean()).backward()
    except torch.cuda.OutOfMemoryError:
        failed.fill_(1)
    model.zero_grad(set_to_none=True)
    dist.all_reduce(failed, op=dist.ReduceOp.MAX)
    if bool(failed):
        import gc
        gc.collect()
        torch.cuda.empty_cache()
        return 512
    return 1024


def finish_metrics(cursor, model, scaler):
    totals = cursor["totals"].clone()
    dist.all_reduce(totals)
    parameters_finite = torch.stack([torch.isfinite(p).all() for p in model.parameters()]).all()
    finite = (cursor["finite"] & parameters_finite).to(torch.int64)
    dist.all_reduce(finite, op=dist.ReduceOp.MIN)
    steps = torch.tensor([cursor["optimizer_steps"], -cursor["optimizer_attempts"],
                          -cursor["amp_skipped_steps"], scaler.get_scale()], device=totals.device, dtype=torch.float64)
    dist.all_reduce(steps, op=dist.ReduceOp.MIN)
    applied, attempts, skipped, scale = steps.tolist()
    values = totals.tolist()
    names = ("policy_loss", "value_loss", "entropy", "kl", "clip_fraction", "loss")
    result = {name: values[i] / max(1, values[6]) for i, name in enumerate(names)}
    result.update(normalized_entropy=values[7] / max(1, values[6]),
                  finite=bool(finite),
                  samples=int(values[6]), optimizer_steps=int(applied), optimizer_attempts=int(-attempts),
                  amp_skipped_steps=int(-skipped), amp_scale=scale)
    return result


def collector_snapshot(collector):
    state = dict(collector.state())
    state["sampling_entropy"] = (collector.search.entropy_sum, collector.search.entropy_count)
    state["timings"] = dict(collector.search.times.seconds)
    state['round_games'] = collector.round_games
    state['episode_faults'] = (dict(collector.search.failed), dict(collector.search.fault_counts))
    if state["partial"] is not None:
        state["partial"] = dict(state["partial"], result=dict(state["partial"]["result"]))
        result = state["partial"]["result"]
        state["partial"]["data_capacity"] = result["data"].numel()
        result["data"] = result["data"][:int(result["offsets"][state["partial"]["next_row"]])]
    return move_tensors(state, torch.device("cpu"))


def restore_collector(collector, state):
    state = dict(state)
    if state["partial"] is not None:
        state["partial"] = dict(state["partial"], result=dict(state["partial"]["result"]))
        result = state["partial"]["result"]
        data = torch.empty(state["partial"].pop("data_capacity"), dtype=torch.uint8, device=collector.search.device)
        data[:len(result["data"])] = result["data"].to(collector.search.device)
        result["data"] = data
    collector.round_games = state.pop('round_games', collector.search.lanes)
    failed, counts = state.pop('episode_faults', ({}, {}))
    collector.search.failed = failed
    collector.search.fault_counts = Counter(counts)
    collector.search.times.seconds = state.pop("timings")
    entropy_sum, entropy_count = state.pop("sampling_entropy")
    collector.restore(state)
    collector.search.entropy_sum = entropy_sum.to(collector.search.device)
    collector.search.entropy_count = entropy_count


def save_rank(path, collector, cursor, update, ppo_rng, round_state):
    state = dict(update=update, round_state=move_tensors(round_state, torch.device("cpu")), collector=collector_snapshot(collector),
                 cursor=move_tensors(cursor, torch.device("cpu")), ppo_rng=ppo_rng.get_state(),
                 torch_rng=torch.get_rng_state(), python_rng=random.getstate(), numpy_rng=np.random.get_state(),
                 cuda_rng=torch.cuda.get_rng_state(collector.search.device) if collector.search.device.type == "cuda" else None)
    temporary = Path(str(path) + ".tmp")
    torch.save(state, temporary)
    temporary.replace(path)


def restore_rank(state, collector, device, ppo_rng):
    restore_collector(collector, state["collector"])
    ppo_rng.set_state(state["ppo_rng"])
    torch.set_rng_state(state["torch_rng"])
    random.setstate(state["python_rng"])
    np.random.set_state(state["numpy_rng"])
    if device.type == "cuda":
        torch.cuda.set_rng_state(state["cuda_rng"], device)
    return move_tensors(state["cursor"], device), state["update"], move_tensors(state["round_state"], device)



def health_stop(state, metrics, faults, games):
    state['high_kl_streak'] = state.get('high_kl_streak', 0) + 1 if metrics['kl'] > .1 else 0
    if not metrics['finite']:
        return 'NaN_or_Inf'
    if metrics['amp_scale'] < 1:
        return 'AMP_scale_below_1'
    if metrics['amp_skipped_steps'] > .5 * metrics['optimizer_attempts']:
        return 'AMP_skipped_more_than_half'
    if faults > .01 * max(1, games):
        return 'episode_fault_rate_above_1_percent'
    if metrics['normalized_entropy'] < .03:
        return 'normalized_entropy_below_0.03'
    if state['high_kl_streak'] >= 3:
        return 'KL_above_0.1_for_3_updates'
    return None
