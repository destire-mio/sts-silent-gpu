"""Stage12: from-scratch CLS policy and no-search PPO."""
from contextlib import nullcontext, contextmanager
from collections import Counter
from copy import deepcopy
import math
import json
import time
from pathlib import Path
import random
import subprocess
import xml.etree.ElementTree as ET

import numpy as np
import torch
from torch import nn
from torch.utils.checkpoint import checkpoint
import torch.distributed as dist

from train.e4 import (RoomSearch, SegmentCollector, PHASE, COMBAT_PHASE, FLOOR, SCREEN, SCREENS,
    ACT, ROOM, BOSS_ROOM, DEATH, VICTORY, ACTION_KINDS)
from train.model import Policy, ModelConfig, network_inputs, distribution, packed_inputs, record_lengths
from train.e4_stage6.training_worker import move_tensors
from train.e4_checkpoint import write_rank_checkpoint
from train.e4_value_diagnostics import gradient_measurement, value_coefficient


DECISION_TYPES = ('combat', 'card_reward', 'map', 'shop', 'campfire', 'other')
MAX_LEARNING_RATE = 4.444444444444445e-5


def decision_type(progress):
    screen = SCREENS[progress[SCREEN]]
    if screen == 'card_reward':
        return 1
    if screen == 'map':
        return 2
    if screen == 'shop' or (screen == 'grid' and progress[ROOM] == 3):
        return 3
    if screen in ('room', 'grid') and progress[ROOM] == 4:
        return 4
    return 0 if progress[PHASE] == COMBAT_PHASE else 5


def decision_entropy_summary(totals):
    sums, counts = totals.tolist()
    return dict(entropy_by_decision_type={name:sums[i] / counts[i] if counts[i] else None
                for i, name in enumerate(DECISION_TYPES)},
                decision_counts_by_type={name:int(counts[i]) for i, name in enumerate(DECISION_TYPES)})


def next_learning_rate(rate, kl):
    if kl > .02:
        rate /= 1.5
    elif kl < .005:
        rate *= 1.5
    return min(MAX_LEARNING_RATE, max(1e-5, rate))


GAE_LAMBDA = .995
GAE_GAMMA = 1.  # The unit discount is implicit in the GAE recurrence below.
DEFAULT_ENTROPY_COEFFICIENT = .01


class CLSPolicy(Policy):
    def __init__(self, config=None):
        config = config or ModelConfig(width=256, layers=4, heads=4, feedforward=1024)
        super().__init__(config)
        self.cls = nn.Parameter(torch.zeros(1, 1, config.width))
        nn.init.normal_(self.cls, std=.02)
        self.encoder_fp16 = False
        self.encoder_dtype_override = None
        self.encoder_checkpoint_enabled = True
        self.encoder_checkpoint_all = True
        if config.width == 768:
            for layer in self.encoder:
                layer.norm_first = True
            self.encoder_norm = nn.LayerNorm(config.width)

    def encode(self, observation):
        mask = observation["entity_mask"]
        device = mask.device
        dtype = self.encoder_dtype_override or torch.float16
        with torch.autocast(device_type=device.type, dtype=dtype,
                            enabled=device.type == "cuda" and (self.encoder_fp16 or self.encoder_dtype_override is not None)):
            values = observation["values"].transpose(1, 2)
            entities = self.numbers(values.sign() * values.abs().log1p())
            for row, embedding in enumerate(self.tags):
                entities = entities + embedding(observation["tags"][:, row])
            for row, embedding in enumerate(self.ids):
                entities = entities + embedding(observation["ids"][:, row])
            entities = self.input_norm(entities)
            entities = torch.cat((self.cls.expand(len(entities), -1, -1), entities), dim=1)
            mask = torch.cat((torch.ones(len(mask), 1, dtype=torch.bool, device=device), mask), dim=1)
            for index, layer in enumerate(self.encoder, 1):
                if (self.encoder_checkpoint_enabled and self.training and torch.is_grad_enabled()
                        and (self.encoder_checkpoint_all or index % 6)):
                    entities = checkpoint(layer, entities, src_key_padding_mask=~mask, use_reentrant=False)
                else:
                    entities = layer(entities, src_key_padding_mask=~mask)
            if self.config.width == 768:
                entities = self.encoder_norm(entities)
        return entities[:, 1:].float(), entities[:, 0].float()


class SeparateCritic(nn.Module):
    encode = CLSPolicy.encode

    def __init__(self, policy):
        super().__init__()
        self.config = policy.config
        self.cls = deepcopy(policy.cls)
        self.tags = deepcopy(policy.tags)
        self.ids = deepcopy(policy.ids)
        self.numbers = deepcopy(policy.numbers)
        self.input_norm = deepcopy(policy.input_norm)
        self.encoder = deepcopy(policy.encoder)
        self.encoder_fp16 = policy.encoder_fp16
        self.encoder_dtype_override = policy.encoder_dtype_override
        self.encoder_checkpoint_enabled = policy.encoder_checkpoint_enabled
        self.encoder_checkpoint_all = policy.encoder_checkpoint_all
        self.combat_value = deepcopy(policy.value)
        self.noncombat_value = deepcopy(policy.value)

    def values(self, pooled, combat):
        return torch.where(combat, self.combat_value(pooled).squeeze(-1),
                           self.noncombat_value(pooled).squeeze(-1))

    def forward(self, observation, combat, *, privileged_tokens=None):
        assert privileged_tokens is None, 'The current critic takes only public observations.'
        _, pooled = self.encode(observation)
        return self.values(pooled, combat)


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
    def __init__(self, lanes, device, seed, *, storage_slots=0, greedy=False, probe_slots=4096):
        super().__init__(lanes, 1, device, torch.Generator(device=device).manual_seed(seed),
                         scratch_slots=storage_slots, inference_chunk=lanes, bosses_seen=511, probe_slots=probe_slots)
        self.greedy = greedy
        self.entropy_sum = torch.zeros((), dtype=torch.float64, device=device)
        self.entropy_count = 0
        self.decision_entropy = torch.zeros((2, len(DECISION_TYPES)), dtype=torch.float64, device=device)
        self.decision_nonforced = torch.zeros((3, len(DECISION_TYPES)), dtype=torch.float64, device=device)
        self.times = StageTimes()
        self.mainline_progress = {}
        self.last_policy_mask = None
        self.training_critic = None
        self.failed = {}
        self.fault_counts = Counter()
        self.midstart_collector = None

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
        if self.midstart_collector is not None:
            record['randomizations'] = deepcopy(self.midstart_collector.randomizations[parent])
            record['start_kind'] = self.midstart_collector.starts[parent]
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
                kinds = torch.tensor([decision_type(self.mainline_progress[slots[i]]) for i in positions],
                                     dtype=torch.int64, device=self.device)
                if self.training_critic is not None:
                    value = self.training_critic(inputs, kinds == 0)
                policy, normalized_entropy = distribution(logits.float(), inputs['candidate_mask'])
                action = policy.probs.argmax(dim=1) if self.greedy else torch.multinomial(policy.probs, 1, generator=self.rng).flatten()
                rows = torch.stack((action.double(), value.double(), policy.log_prob(action).double()), dim=1).cpu().tolist()
                self.entropy_sum += policy.entropy().double().sum()
                self.entropy_count += len(positions)
                self.decision_entropy[0].scatter_add_(0, kinds, normalized_entropy.double())
                self.decision_entropy[1].scatter_add_(0, kinds, torch.ones_like(normalized_entropy, dtype=torch.float64))
                nonforced = inputs["candidate_mask"].sum(-1) > 1
                self.decision_nonforced[0].scatter_add_(0, kinds, normalized_entropy.double() * nonforced)
                self.decision_nonforced[1].scatter_add_(0, kinds, nonforced.double())
                self.decision_nonforced[2].scatter_add_(0, kinds, (~nonforced).double())
            for i, row in zip(positions, rows):
                choices[i], values[i], logs[i] = int(row[0]), row[1], row[2]
        return choices, values, logs

    @torch.no_grad()
    def scores(self, model, slots, progress):
        if self.training_critic is None:
            return super().scores(model, slots, progress)
        result = dict.fromkeys(slots, 0.)
        alive = [slot for slot in slots if progress[slot][SCREEN] != DEATH]
        if alive:
            index = torch.tensor(alive, dtype=torch.int64, device=self.device)
            inputs = network_inputs(self.arrays, self.device, index)
            model(inputs)  # Preserve the auxiliary capture used by the arena and rollout endpoints.
            combat = torch.tensor([decision_type(progress[slot]) == 0 for slot in alive], device=self.device)
            result.update(zip(alive, self.training_critic(inputs, combat).cpu().tolist()))
        return result


class NoSLCollector(SegmentCollector):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.round_games = 0
        self.search.reward_ledgers = self.rewards

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
                        reward=self.rewards[slot].total / self.rewards[slot].reward_budget))
                    self.seeds[slot] = None
            return rewards, terminal

    @torch.no_grad()
    def collect(self, model, length=64, *, after_step=None, auxiliary_weights=None, track_value_acts=False):
        device, lanes = self.search.device, self.search.lanes
        slots = list(range(lanes))
        if self.partial is None:
            capacity = 10240
            result = dict(data=torch.empty(length * lanes * capacity, dtype=torch.uint8, device=device),
                          offsets=torch.zeros(length * lanes + 1, dtype=torch.int64, device=device),
                          decision_types=torch.zeros(length * lanes, dtype=torch.int64, device=device),
                          policy_mask=torch.empty((length, lanes, 128), dtype=torch.bool, device=device))
            for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "action_valid", "combat"):
                dtype = torch.int64 if name == "actions" else torch.bool if name in ("terminal", "imitation", "action_valid", "combat") else torch.float32
                result[name] = torch.zeros((length, lanes), dtype=dtype, device=device)
            if track_value_acts:
                result["value_acts"] = torch.zeros((length, lanes), dtype=torch.int64, device=device)
            self.partial = dict(result=result, next_step=0, next_row=0, length=length,
                                completed_before=len(self.completed))
            if auxiliary_weights:
                from train.e4_stage12_auxiliary import rollout_storage
                self.partial["auxiliary"] = rollout_storage(auxiliary_weights, length, lanes, device)
        auxiliary = self.partial.get("auxiliary")
        if auxiliary:
            from train.e4_stage12_auxiliary import record_decision, record_outcome, finish_rollout
        result = self.partial["result"]
        after_transition = getattr(self, "after_transition", None)
        for tick in range(self.partial["next_step"], length):
            self.search.clock()
            fresh = [s for s in slots if self.seeds[s] is None]
            if fresh:
                self.fresh(fresh)
            progress = self.search.progress_for(slots)
            if auxiliary:
                actions, values, logs, imitation = record_decision(auxiliary, self, model, slots, progress, tick)
            else:
                actions, values, logs, imitation = self.choose(model, slots, progress)
            if "value_acts" in result:
                result["value_acts"][tick] = torch.tensor([progress[slot][ACT] for slot in slots], device=device)
            result["policy_mask"][tick] = self.search.last_policy_mask
            if "decision_types" not in result:
                result["decision_types"] = torch.full((length * lanes,), -1, dtype=torch.int64, device=device)
            result["decision_types"][tick * lanes:(tick+1)*lanes] = torch.tensor([decision_type(progress[s]) for s in slots],device=device)
            row = tick * lanes
            index = torch.tensor(slots, dtype=torch.int64, device=device)
            lengths = (4 + 24 * self.search.arrays["entity_counts"][index].long()
                       + 7 * self.search.arrays["candidate_counts"][index].long())
            result["offsets"][row + 1:row + lanes + 1] = result["offsets"][row] + lengths.cumsum(0)
            self.search.batch.store_tensors(index, result["data"], result["offsets"][row:row + lanes + 1])
            reward, terminal = self.advance(slots, actions, progress)
            if after_transition is not None:
                after_transition(model, slots, progress, reward, terminal, auxiliary)
            if auxiliary:
                record_outcome(auxiliary, self, slots, progress, terminal, tick)
            for name, value in (("actions", actions), ("values", values), ("logs", logs), ("rewards", reward),
                                ("terminal", terminal), ("imitation", imitation),
                                ("action_valid", [s not in self.stalled for s in slots]),
                                ("combat", [progress[s][PHASE] == COMBAT_PHASE for s in slots])):
                result[name][tick] = torch.as_tensor(value, device=device, dtype=result[name].dtype)
            self.partial["next_step"] = tick + 1
            self.partial["next_row"] = (tick + 1) * lanes
            if after_step is not None:
                after_step()
        live = [s for s in slots if self.seeds[s] is not None]
        bootstrap = torch.zeros(lanes, device=device)
        if auxiliary:
            progress = self.search.progress_for(live) if live else {}
            result["auxiliary"], result["auxiliary_metrics"] = finish_rollout(
                auxiliary, self, model, live, progress, bootstrap)
        elif live:
            scores = self.search.scores(model, live, self.search.progress_for(live))
            bootstrap[live] = torch.tensor([scores[s] for s in live], device=device)
        advantages = torch.zeros_like(result["values"])
        carry = torch.zeros(lanes, device=device)
        for tick in reversed(range(length)):
            continuing = ~result["terminal"][tick]
            delta = result["rewards"][tick] + bootstrap * continuing - result["values"][tick]
            carry = delta + GAE_LAMBDA * continuing * carry
            advantages[tick] = carry
            bootstrap = result["values"][tick]
        result["returns"] = result["values"] + advantages
        result["advantages"] = advantages
        result["policy_mask"] = result["policy_mask"].flatten(0, 1)
        for name in ("actions", "values", "logs", "rewards", "terminal", "imitation", "action_valid", "combat", "advantages", "returns"):
            result[name] = result[name].flatten()
        if "value_acts" in result:
            result["value_acts"] = result["value_acts"].flatten()
        result["data"] = result["data"][:int(result["offsets"][-1])].clone()
        self.partial = None
        return result


def distillation_terms(logits, value, teacher_logits, teacher_value, legal):
    teacher_log = teacher_logits.float().log_softmax(-1).masked_fill(~legal, 0.)
    student_log = logits.float().log_softmax(-1).masked_fill(~legal, 0.)
    probability = teacher_log.exp().masked_fill(~legal, 0.)
    return (probability * (teacher_log - student_log)).sum(-1), (value - teacher_value).square()


@torch.no_grad()
def student_log_baseline(student, fragment, microbatch=1024):
    indices = fragment['action_valid'].nonzero().flatten()
    for ids in indices.split(microbatch):
        inputs = packed_inputs(fragment['data'], fragment['offsets'], ids, ids.device)
        inputs['candidate_mask'] &= fragment['policy_mask'][ids, :inputs['candidate_mask'].shape[1]]
        logits, _ = student(inputs)
        policy, _ = distribution(logits.float(), inputs['candidate_mask'])
        fragment['logs'][ids] = policy.log_prob(fragment['actions'][ids])


PPO_GLOBAL_MINIBATCH = 32768
DISTILLATION_GLOBAL_MINIBATCH = 4096


def minibatch_order(counts, rng, device, global_minibatch):
    order = torch.randperm(sum(counts), generator=rng, device=device)
    if global_minibatch == DISTILLATION_GLOBAL_MINIBATCH:
        # Keep the shuffled order within each rank while giving each rank its own quota.
        ends = torch.tensor(counts, device=device).cumsum(0)
        owners = torch.bucketize(order, ends, right=True)
        order = order[torch.argsort(owners, stable=True)]
    return order


def distillation_rank_quotas(world):
    quota, remainder = divmod(DISTILLATION_GLOBAL_MINIBATCH, world)
    return [quota + (rank < remainder) for rank in range(world)]


def new_cursor(fragment, rng, world, *, global_minibatch=PPO_GLOBAL_MINIBATCH):
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
                global_minibatch=global_minibatch,
                order=minibatch_order(counts, rng, device, global_minibatch),
                totals=torch.zeros(8, dtype=torch.float64, device=device),
                finite=torch.ones((), dtype=torch.bool, device=device), optimizer_steps=0,
                optimizer_attempts=0, amp_skipped_steps=0)


def local_minibatch(cursor, rank):
    size = cursor.get("global_minibatch", PPO_GLOBAL_MINIBATCH)
    counts = cursor["counts"]
    order = cursor["order"]
    offset = sum(counts[:rank])
    if size == DISTILLATION_GLOBAL_MINIBATCH:
        quotas = distillation_rank_quotas(len(counts))
        batch = cursor["start"] // size
        start = min(counts[rank], batch * quotas[rank])
        end = min(counts[rank], start + quotas[rank])
        owned = order[offset + start:offset + end] - offset
        global_count = sum(max(0, min(quota, count - batch * quota))
                           for count, quota in zip(counts, quotas))
    else:
        ids = order[cursor["start"]:cursor["start"] + size]
        owned = ids[(ids >= offset) & (ids < offset + counts[rank])] - offset
        global_count = len(ids)
    selected = cursor["indices"][owned]
    f = cursor["fragment"]
    if len(selected):
        entities, candidates = record_lengths(f["data"], f["offsets"], selected)
        selected = selected[torch.argsort((entities // 16) * 129 + candidates, stable=True)]
    return selected, global_count


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
    values = totals.tolist()
    mean, clipped = values[0] / max(1, values[2]), values[1] / max(1, values[2])
    model.encoder_fp16 = math.isfinite(mean) and mean <= .005 and clipped <= .01
    return dict(mean_abs_log_ratio=mean, clip_fraction=clipped,
                precision="encoder_fp16_heads_fp32" if model.encoder_fp16 else "fp32")


@torch.no_grad()
def decision_training_summary(cursor):
    fragment = cursor["fragment"]
    kinds = fragment.get("decision_types")
    if kinds is None:
        kinds = torch.full_like(fragment["actions"], -1)
    eligible = (fragment["action_valid"] & (kinds >= 0)
                & (fragment["policy_mask"].sum(-1, dtype=torch.int32) > 1))
    indices = kinds[eligible]
    advantage = fragment["advantages"][eligible].double()
    actor = cursor.get("decision_diagnostics_actor")
    if actor is None:
        actor_count = actor_sum = torch.zeros_like(advantage)
    else:
        losses = actor[:, eligible]
        recorded = torch.isfinite(losses)
        actor_count = recorded.sum(dim=0)
        actor_sum = losses.masked_fill(~recorded, 0.).double().sum(dim=0)
    totals = advantage.new_zeros((len(DECISION_TYPES), 5))
    totals.index_add_(0, indices, torch.stack((torch.ones_like(advantage), advantage,
                                              advantage.square(), actor_count, actor_sum), dim=1))
    drift = cursor.get("decision_diagnostics_drift", totals.new_zeros((len(DECISION_TYPES), 3)))
    result = {}
    for kind, moments, (compared, kl, flipped) in zip(DECISION_TYPES, totals.tolist(), drift.tolist()):
        samples, total, squares, actor_samples, loss = moments
        mean = total / samples if samples else None
        result[kind] = dict(samples=int(samples), advantage_mean=mean,
                            advantage_std=math.sqrt(max(0., squares / samples - mean**2)) if samples else None,
                            policy_loss=loss / actor_samples if actor_samples else None,
                            policy_loss_samples=int(actor_samples), final_minibatch_samples=int(compared),
                            post_pre_kl=max(0., kl / compared) if compared else None,
                            argmax_flip_rate=flipped / compared if compared else None)
    return result


@torch.no_grad()
def measure_decision_drift(model, fragment, before_step):
    device = fragment["actions"].device
    totals = torch.zeros(len(DECISION_TYPES), 3, dtype=torch.float64, device=device)
    with torch.random.fork_rng(devices=[device.index] if device.type == "cuda" else []):
        for ids, before in before_step:
            inputs = packed_inputs(fragment["data"], fragment["offsets"], ids, device)
            legal = inputs["candidate_mask"]
            legal &= fragment["policy_mask"][ids, :legal.shape[1]]
            # Reuse the learner forward backend even when the encoder is compiled.
            with torch.enable_grad():
                logits, value = model(inputs)
            after, _ = distribution(logits.detach().float(), legal)
            del logits, value
            kl = (after.probs * (after.logits.masked_fill(~legal, 0.)
                                 - before.masked_fill(~legal, 0.))).sum(dim=1)
            flipped = after.logits.argmax(dim=1) != before.argmax(dim=1)
            kinds = fragment["decision_types"][ids]
            eligible = (kinds >= 0) & (legal.sum(dim=1) > 1) & torch.isfinite(kl)
            stats = torch.stack((torch.ones_like(kl), kl, flipped), dim=1)
            totals.index_add_(0, kinds[eligible], stats[eligible].double())
    return totals


def advance_minibatch(model, optimizer, scaler, cursor, rng, rank, world, microbatch=1024,
                      teacher=None, entropy_coefficient=DEFAULT_ENTROPY_COEFFICIENT, learner_profile=None,
                      value_state=None, diagnostic_log=None, expert_oom_callback=None, auxiliary_weights=None,
                      decision_diagnostics=False, critic=None, critic_optimizer=None, critic_scaler=None):
    f = cursor["fragment"]
    auxiliary = f.get("auxiliary") if auxiliary_weights else None
    if auxiliary:
        from train.e4_stage12_auxiliary import auxiliary_loss
    selected, global_count = local_minibatch(cursor, rank)
    chunks = list(selected.split(microbatch))
    actor_record = None
    probe = None
    if decision_diagnostics:
        actor_record = cursor.get("decision_diagnostics_actor")
        if actor_record is None:
            actor_record = f["advantages"].new_full((2, len(f["actions"])), float("nan"))
            cursor["decision_diagnostics_actor"] = actor_record
        minibatch = cursor.get("global_minibatch", PPO_GLOBAL_MINIBATCH)
        if "decision_types" in f and cursor["epoch"] == 1 and cursor["start"] + minibatch >= sum(cursor["counts"]):
            probe = []
    count = torch.tensor(len(chunks), device=cursor["order"].device)
    dist.all_reduce(count, op=dist.ReduceOp.MAX)
    optimizer.zero_grad(set_to_none=True)
    if critic is not None:
        assert teacher is None and world == 1 and value_state is not None
        critic_optimizer.zero_grad(set_to_none=True)
        critic_coefficient = value_coefficient(value_state)
    steps = int(count)
    manual = value_state is not None and value_state.get("standalone_value_coefficient") is not None
    recalibrate_ppo = teacher is None and value_state is not None and value_state.get("claude171_value_calibration_pending")
    if critic is None and value_state is not None and not manual and ("value_calibration" not in value_state or recalibrate_ppo):
        calibration = torch.zeros(4, device=selected.device, dtype=torch.float64)
        owner = 0
        ids = selected[:microbatch]
        if recalibrate_ppo:
            eligible = (f["policy_mask"][selected].sum(dim=1) > 1) & (f["advantages"][selected] != 0)
            ids = selected[eligible][:microbatch]
            candidate_rank = torch.tensor(rank if len(ids) else world, device=selected.device)
            dist.all_reduce(candidate_rank, op=dist.ReduceOp.MIN)
            owner = int(candidate_rank)
        if rank == owner:
            assert len(ids) > 0
            inputs = packed_inputs(f["data"], f["offsets"], ids, ids.device)
            inputs["candidate_mask"] &= f["policy_mask"][ids, :inputs["candidate_mask"].shape[1]]
            with model.no_sync():
                measured = gradient_measurement(
                    model.module, inputs, f["actions"][ids], f["logs"][ids], f["advantages"][ids],
                    f["returns"][ids], scale=scaler.get_scale(), nonforced_only=bool(recalibrate_ppo))
            if measured is not None:
                calibration[:] = torch.tensor(
                    [measured[k] for k in ("actor_encoder_norm", "weighted_value_encoder_norm", "ratio", "target")],
                    device=ids.device, dtype=torch.float64)
        if owner < world:
            dist.broadcast(calibration, src=owner)
        measured = dict(zip(("actor_encoder_norm", "weighted_value_encoder_norm", "ratio", "target"),
                            calibration.tolist()))
        if recalibrate_ppo and (not bool(torch.isfinite(calibration).all()) or measured["actor_encoder_norm"] <= 0):
            if rank == 0 and not value_state.get("claude171_deferred_logged"):
                diagnostic_log(dict(event="claude171_value_calibration_deferred",
                                    update=value_state["update"], informative_rank=owner if owner < world else None,
                                    actor_encoder_norm=measured["actor_encoder_norm"] if bool(torch.isfinite(calibration[0])) else None))
            value_state["claude171_deferred_logged"] = True
        else:
            measured.update(measured_value_coefficient=.5, completed_updates=0)
            if recalibrate_ppo:
                previous = value_coefficient(value_state)
                start_coefficient = measured["target"] if value_state.get("claude186_control") else previous
                measured["start_coefficient"] = start_coefficient
                report = dict(start_update=value_state["update"] + 1, old_coefficient=previous,
                              new_coefficient=measured["target"], ratio=measured["ratio"], milestones={},
                              measurement_rank=owner, actor_encoder_norm=measured["actor_encoder_norm"],
                              weighted_value_encoder_norm=measured["weighted_value_encoder_norm"])
                if value_state.get("claude186_control"):
                    report["applied_start_coefficient"] = start_coefficient
                value_state["claude171_value_calibration"] = report
                value_state.pop("claude171_value_calibration_pending")
                value_state.pop("claude171_deferred_logged", None)
                if rank == 0:
                    diagnostic_log(dict(event="claude171_value_recalibrated", **report))
            elif rank == 0:
                diagnostic_log(dict(event="value_gradient_calibration", **measured))
            value_state["value_calibration"] = measured
    ce_gradients = None
    if cursor.get("expert_total"):
        from train.e4_expert_iteration import expert_backward
        ce_gradients = expert_backward(model, scaler, cursor, rank, world, microbatch, expert_oom_callback)
    for i in range(steps):
        if learner_profile is not None:
            learner_profile.before_microbatch()
        ids = chunks[i] if i < len(chunks) else selected[:0]
        sync = model.no_sync() if i + 1 < steps else nullcontext()
        with sync:
            if len(ids):
                inputs = packed_inputs(f["data"], f["offsets"], ids, ids.device)
                inputs["candidate_mask"] &= f["policy_mask"][ids, :inputs["candidate_mask"].shape[1]]
                if learner_profile is not None:
                    learner_profile.observe(inputs["entity_mask"])
                if teacher is not None:
                    with torch.no_grad():
                        teacher_logits, teacher_value = teacher(inputs)
                if auxiliary:
                    logits, value, predictions = model(inputs, auxiliary=True)
                else:
                    logits, value = model(inputs)
                policy, normalized_entropy = distribution(logits.float(), inputs["candidate_mask"])
                log_ratio = policy.log_prob(f["actions"][ids]) - f["logs"][ids]
                ratio = log_ratio.exp()
                actor = -torch.minimum(ratio * f["advantages"][ids],
                                       ratio.clamp(.8, 1.2) * f["advantages"][ids])
                if critic is None:
                    value_loss = (value - f["returns"][ids]).square()
                else:
                    del value
                    value_loss = torch.zeros_like(actor)
                if teacher is None:
                    if critic is None:
                        loss = actor + (value_coefficient(value_state) if value_state is not None else .5) * value_loss - entropy_coefficient * normalized_entropy
                    else:
                        loss = actor - entropy_coefficient * normalized_entropy
                else:
                    actor, value_loss = distillation_terms(logits, value, teacher_logits, teacher_value, inputs['candidate_mask'])
                    loss = actor + (value_coefficient(value_state) if value_state is not None else .5) * value_loss
                if auxiliary:
                    loss = loss + auxiliary_loss(predictions, auxiliary, ids, auxiliary_weights)
                if actor_record is not None:
                    actor_record[cursor["epoch"]].index_copy_(0, ids, actor.detach())
                if probe is not None:
                    probe.append((ids, policy.logits.detach()))
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
        if critic is not None and len(ids):
            predicted = critic(inputs, f["decision_types"][ids] == 0)
            mse = (predicted - f["returns"][ids]).square()
            weighted = critic_coefficient * mse.sum()
            critic_scaler.scale(weighted / max(1, global_count)).backward()
            cursor["finite"] &= torch.isfinite(mse).all()
            cursor["totals"][1] += mse.detach().double().sum()
            cursor["totals"][5] += weighted.detach().double()
        if learner_profile is not None:
            learner_profile.after_microbatch()
    if 'expert_total' in cursor:
        from train.e4_expert_iteration import add_capped_expert_gradients
        add_capped_expert_gradients(model, ce_gradients, scaler, cursor, world)
        del ce_gradients
    critic_stepped = False
    if critic is not None:
        critic_scaler.unscale_(critic_optimizer)
        torch.nn.utils.clip_grad_norm_(critic.parameters(), .5, error_if_nonfinite=False)
        scale = critic_scaler.get_scale()
        critic_scaler.step(critic_optimizer)
        critic_scaler.update()
        critic_stepped = critic_scaler.get_scale() >= scale
    finish_optimizer_step(model, optimizer, scaler, cursor, rng, rank)
    if probe:
        cursor["decision_diagnostics_drift"] = measure_decision_drift(model.module, f, probe)
    return critic_stepped


def finish_optimizer_step(model, optimizer, scaler, cursor, rng, rank):
    scaler.unscale_(optimizer)
    norm = torch.nn.utils.clip_grad_norm_(model.parameters(), .5, error_if_nonfinite=False)
    stats = cursor.setdefault("gradient_stats", [0.,0.,0.,0.])
    stats[0] += float(norm)
    stats[1] = max(stats[1], float(norm))
    stats[2] += int(float(norm) > .5)
    stats[3] += 1
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
    advance_cursor(cursor, rng)


def advance_cursor(cursor, rng):
    size = cursor.get("global_minibatch", PPO_GLOBAL_MINIBATCH)
    counts = cursor["counts"]
    cursor["start"] += size
    if size == DISTILLATION_GLOBAL_MINIBATCH:
        quotas = distillation_rank_quotas(len(counts))
        batches = max((count + quota - 1) // quota for count, quota in zip(counts, quotas))
        epoch_end = batches * size
    else:
        epoch_end = sum(counts)
    if cursor["start"] >= epoch_end:
        cursor["epoch"] += 1
        cursor["start"] = 0
        if cursor["epoch"] < 2:
            cursor["order"] = minibatch_order(counts, rng, cursor["order"].device, size)


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
    stats = torch.tensor(cursor.get("gradient_stats",[0.,0.,0.,0.]),device=totals.device,dtype=torch.float64)
    maximum = stats[1].clone()
    dist.all_reduce(stats)
    dist.all_reduce(maximum,op=dist.ReduceOp.MAX)
    result.update(gradient_norm_before_clip_mean=float(stats[0]/stats[3]) if float(stats[3]) else None,gradient_norm_before_clip_max=float(maximum),gradient_clipped_fraction=float(stats[2]/stats[3]) if float(stats[3]) else None)
    return result


def collector_snapshot(collector):
    state = dict(collector.state())
    state["sampling_entropy"] = (collector.search.entropy_sum, collector.search.entropy_count)
    state["decision_entropy"] = collector.search.decision_entropy
    state["decision_nonforced"] = collector.search.decision_nonforced
    state["timings"] = dict(collector.search.times.seconds)
    state['round_games'] = collector.round_games
    state['episode_faults'] = (dict(collector.search.failed), dict(collector.search.fault_counts))
    if hasattr(collector, "auxiliary_fresh"):
        state["auxiliary_fresh"] = collector.auxiliary_fresh
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
    collector.search.decision_entropy = state.pop('decision_entropy',
        torch.zeros_like(collector.search.decision_entropy)).to(collector.search.device)
    collector.search.decision_nonforced = state.pop('decision_nonforced', torch.zeros_like(collector.search.decision_nonforced)).to(collector.search.device)
    collector.round_games = state.pop('round_games', collector.search.lanes)
    failed, counts = state.pop('episode_faults', ({}, {}))
    collector.search.failed = failed
    collector.search.fault_counts = Counter(counts)
    collector.search.times.seconds = state.pop("timings")
    entropy_sum, entropy_count = state.pop("sampling_entropy")
    fresh = state.pop("auxiliary_fresh", None)
    if fresh is not None:
        collector.auxiliary_fresh = fresh
    if state["partial"] is not None and "auxiliary" in state["partial"]:
        state["partial"]["auxiliary"] = move_tensors(state["partial"]["auxiliary"], collector.search.device)
    collector.restore(state)
    collector.search.reward_ledgers = collector.rewards
    collector.search.entropy_sum = entropy_sum.to(collector.search.device)
    collector.search.entropy_count = entropy_count


def save_rank(path, collector, cursor, update, ppo_rng, round_state, committed_path=None, expert_state=None,
              *, retain_by_copy=False):
    state = dict(update=update, expert=expert_state, round_state=move_tensors(round_state, torch.device("cpu")), collector=collector_snapshot(collector),
                 cursor=move_tensors(cursor, torch.device("cpu")), ppo_rng=ppo_rng.get_state(),
                 torch_rng=torch.get_rng_state(), python_rng=random.getstate(), numpy_rng=np.random.get_state(),
                 cuda_rng=torch.cuda.get_rng_state(collector.search.device) if collector.search.device.type == "cuda" else None)
    write_rank_checkpoint(path, state, torch.save, committed_path, retain_by_copy=retain_by_copy)


def restart_collector_observation(saved, fresh):
    state = deepcopy(fresh)
    for key in ('seed_rng', 'search_rng', 'decisions'):
        state[key] = saved[key]
    state['midstart']['pools'] = deepcopy(saved['midstart']['pools'])
    return state


def restart_rank_observation(state, collector):
    fresh = restart_collector_observation(state['collector'], collector_snapshot(collector))
    return dict(state, collector=fresh, cursor=None,
                round_state=dict(ready=None, seconds=0., decisions_start=fresh['decisions']))


def restart_rank_engine(state, collector):
    fresh = collector_snapshot(collector)
    for key in ('seed_rng', 'search_rng', 'decisions'):
        fresh[key] = state['collector'][key]
    return dict(state, collector=fresh, cursor=None,
                round_state=dict(ready=None, seconds=0., decisions_start=fresh['decisions']))


def pandora_cuda_gates(build, receipt):
    names = ('test_cuda_dispatch', 'test_cuda_effect_fields', 'test_cuda_faults',
             'test_cuda_parity', 'test_cuda_root_cache')
    subprocess.run(['ctest', '--test-dir', str(build), '-R', '^(' + '|'.join(names) + ')$',
                    '--parallel', '1', '--output-on-failure', '--output-junit', str(receipt)], check=True)
    cases = list(ET.parse(receipt).getroot().iter('testcase'))
    assert len(cases) == len(names) and {case.attrib['name'] for case in cases} == set(names)
    assert all(case.find('skipped') is None and case.find('failure') is None
               and case.find('error') is None and case.attrib.get('status') == 'run' for case in cases)


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
