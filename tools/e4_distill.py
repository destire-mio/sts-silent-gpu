import argparse
from contextlib import nullcontext
from copy import deepcopy
from dataclasses import asdict
from datetime import timedelta
import gzip
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.checkpoint import engine_commit
from train.e4 import (AllocationEnded, CAPACITY_METRICS, LOOP_METRICS, REACH, SEARCH_METRICS,
                      RoomSearch, SegmentCollector, combat_diagnostics_summary)
from train.e4_learner import update, update_beta
from train.e4_rewards import noncombat_summary
from train.model import ModelConfig, Policy, packed_inputs, store_inputs
from tools.e4_continue import waiting
from tools.e4_run import atomic_checkpoint, atomic_json, audit, collective_stop, summaries


TEACHER_COMMIT = '0722b1b4e2a2c2dadda19e5b1eea7864363073c2'
TEACHER_HASHES = (
    '2bac4afd004002a773642f9671482cd0b9aa71c8b9c59ca2ba1713ffa02eefc6',
    '276586824760c171c277ada13d94e2c72faf40cc73ed5e751dfa8b34727e7893',
    '19a9f2c30ac5d0f0f533c874f946d76c0b95b17cebc9fb6e5c5c256d156bab74',
    '58976667069ced8a0ac0db7510d4988fb14b28180443102e3a86692e6e9eb94c',
    'e1a371d29dbdc29a7bc63fd8c98a0af4fa155d559d539de9f454069812d7bfad',
    'a9365bafee30bbc4ff7343e0a37c50783e8ec777bd90becd33e8c21290cc3f73',
    'a7d321f9e9ffe3856ee9347c321733f67b5fbbedb74ed5c9372c06777e0bb7c6')
LARGE = ModelConfig(width=256, layers=6, heads=8, feedforward=1024)
MEDIUM = ModelConfig(width=192, layers=6, heads=6, feedforward=768)
BASELINE = dict(average_floor=19.4931640625,
                reach=dict(zip(REACH, (607/1024, 4/1024, 0., 0., 0., 0., 0., 0.))))


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def teacher_source(directory, rank=0):
    hashes = [sha256(directory / f'rank-{r}.pt') for r in range(7)]
    assert tuple(hashes) == TEACHER_HASHES, 'C180 source files changed'
    source = torch.load(directory / f'rank-{rank % 7}.pt', map_location='cpu', weights_only=False)
    assert source['commit'] == TEACHER_COMMIT and source['version'] == 180
    assert source['phase'] == 'evaluation' and source['reason'] == 'before_evaluation'
    assert source['world_size'] == 7 and source['lanes'] == 256 and source['trials'] == 32
    assert source['initialization'] == 1729 and source['epochs'] == 4
    assert source['ready'] is None and source['train_state']['partial'] is None
    assert source['imitation_beta'] == .0837332699980915
    assert source['imitation_entropy_target'] == .6716025225602738
    return source, hashes


def new_collector(device, rank):
    search_rng = torch.Generator(device=device).manual_seed(2729 + rank)
    seed_rng = torch.Generator().manual_seed(3729 + rank)
    search = RoomSearch(256, 32, device, search_rng, scratch_slots=256*33)
    return SegmentCollector(search, seed_rng, rank=rank)


def select_benchmark(rows):
    assert 2 <= len(rows) <= 3
    expected = (('small', ModelConfig()), ('large', LARGE), ('medium', MEDIUM))
    for row, (name, config) in zip(rows, expected):
        assert row['name'] == name and row['config'] == asdict(config)
        assert row['seconds'] >= 300 and row['actual_steps'] > 0
        assert row['actual_steps_per_second'] == row['actual_steps']/row['seconds']
    small = rows[0]
    for row in rows[1:]:
        if row['actual_steps_per_second'] >= .5 * small['actual_steps_per_second']:
            return row['config']
    return None


def benchmark(args, source, hashes, device):
    assert dist.get_world_size() == 1 and 0 < args.seconds <= 1200
    started = time.monotonic()
    deadline = min(started + args.seconds,
                   float(os.environ.get('C76_ALLOCATION_DEADLINE_MONOTONIC', 'inf'))) - 30
    commit = engine_commit()
    saved = torch.load(args.resume / 'rank-0.pt', map_location='cpu', weights_only=False) if args.resume else None
    state = dict(rows=[], index=0, sample_seconds=0., sample_steps=0, sample_sl_steps=0,
                 peak_allocated=0, peak_reserved=0)
    if saved:
        assert saved['commit'] == commit and saved['teacher_hashes'] == hashes
        assert saved['world_size'] == 1 and saved['task'] == 'benchmark'
        state = saved['state']
    collector = None
    policy = None

    def save(reason):
        atomic_checkpoint(args.output / 'rank-0.pt', dict(task='benchmark', commit=commit,
            teacher_hashes=hashes, source_rank_mapping=[0], world_size=1, state=state,
            collector=collector.state() if collector else None,
            torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device)))
        atomic_json(args.output / 'status.json', dict(task='benchmark', reason=reason, **state))

    while state['index'] < 3:
        if state['index'] == 2 and select_benchmark(state['rows']) is not None:
            break
        name, config = (('small', ModelConfig()), ('large', LARGE), ('medium', MEDIUM))[state['index']]
        torch.manual_seed(1729)
        policy = Policy(config).to(device).eval()
        if name == 'small':
            policy.load_state_dict(source['model'])
        collector = new_collector(device, 0)
        collector.search.evidence_directory = args.output
        if saved and saved['collector'] is not None:
            collector.restore(saved['collector'])
            torch.set_rng_state(saved['torch_rng'])
            torch.cuda.set_rng_state(saved['cuda_rng'], device)
        saved = None
        torch.cuda.reset_peak_memory_stats(device)
        while state['sample_seconds'] < 300:
            if waiting(0, args.output, state['index']):
                save('yield_to_waiting_user')
                return
            if time.monotonic() >= deadline:
                save('allocation_limit')
                return
            before, sl_before = collector.decisions, collector.search.searched_decisions
            start = time.monotonic()
            collector.search.deadline = min(deadline, start + 300 - state['sample_seconds'])
            expired = False
            try:
                fragment = collector.collect(policy)
                del fragment
            except AllocationEnded:
                expired = True
            torch.cuda.synchronize(device)
            state['sample_seconds'] += time.monotonic() - start
            state['sample_steps'] += collector.decisions - before
            state['sample_sl_steps'] += collector.search.searched_decisions - sl_before
            state['peak_allocated'] = max(state['peak_allocated'], torch.cuda.max_memory_allocated(device))
            state['peak_reserved'] = max(state['peak_reserved'], torch.cuda.max_memory_reserved(device))
            if expired and time.monotonic() >= deadline and state['sample_seconds'] < 300:
                save('allocation_limit')
                return
        row = dict(name=name, config=asdict(config), seconds=state['sample_seconds'],
            actual_steps=state['sample_steps'], sl_trial_steps=state['sample_sl_steps'],
            actual_steps_per_second=state['sample_steps']/state['sample_seconds'],
            sl_trial_steps_per_second=state['sample_sl_steps']/state['sample_seconds'],
            peak_torch_allocated_bytes=state['peak_allocated'],
            peak_torch_reserved_bytes=state['peak_reserved'],
            boundary_overshoot_seconds=max(0., state['sample_seconds']-300),
            boundary='RoomSearch.clock_before_next_actual_or_SL_step')
        state['rows'].append(row)
        print(json.dumps(dict(event='benchmark', **row)), flush=True)
        state.update(index=state['index']+1, sample_seconds=0., sample_steps=0, sample_sl_steps=0,
                     peak_allocated=0, peak_reserved=0)
        collector, policy = None, None
        torch.cuda.empty_cache()
        save('sample_complete')
    chosen = select_benchmark(state['rows'])
    atomic_json(args.output / 'selection.json', dict(config=chosen, rows=state['rows'],
        teacher_hashes=hashes, source_rank_mapping=[0], commit=commit,
        passed=chosen is not None, criterion='largest_actual_decisions_per_second_at_least_half_C180'))
    save('benchmark_complete' if chosen else 'no_model_at_half_C180_throughput')


def held_out(seed):
    return int.from_bytes(hashlib.sha256(f'1729:{seed}'.encode()).digest()[:8], 'little') % 20 == 0


class DistillCollector(SegmentCollector):
    def __init__(self, search, seed_rng, *, rank=0):
        super().__init__(search, seed_rng, rank=rank)
        self.decision_seeds = []
        self.decision_episodes = []
        self.episode_counter = 0
        self.episode_ids = [None] * search.lanes

    def reset(self, slots, seeds):
        super().reset(slots, seeds)
        for slot in slots:
            self.episode_ids[slot] = (self.rank << 48) + self.episode_counter
            self.episode_counter += 1

    def choose(self, model, slots, progress):
        result = super().choose(model, slots, progress)
        self.decision_seeds.extend(self.seeds[s] for s in slots)
        self.decision_episodes.extend(self.episode_ids[s] for s in slots)
        return result

    def advance(self, slots, actions, progress):
        before = len(self.completed)
        reward, terminal = super().advance(slots, actions, progress)
        ids = [self.episode_ids[s] for s, ended in zip(slots, terminal) if ended]
        for episode, identity in zip(self.completed[before:], ids):
            episode['distill_episode_id'] = identity
        return reward, terminal

    def state(self):
        return dict(super().state(), decision_seeds=self.decision_seeds,
                    decision_episodes=self.decision_episodes, episode_counter=self.episode_counter,
                    episode_ids=self.episode_ids)

    def restore(self, state):
        super().restore(state)
        self.decision_seeds = state['decision_seeds']
        self.decision_episodes = state['decision_episodes']
        self.episode_counter = state['episode_counter']
        self.episode_ids = state['episode_ids']


def compact_rows(fragment, indices):
    indices = indices.cpu()
    offsets = fragment['offsets'].cpu()
    data = fragment['data'].cpu()
    pieces = [data[int(offsets[i]):int(offsets[i+1])] for i in indices.tolist()]
    lengths = torch.tensor([len(piece) for piece in pieces], dtype=torch.int64)
    result = dict(data=torch.cat(pieces) if pieces else torch.empty(0, dtype=torch.uint8),
                  offsets=torch.cat((torch.zeros(1, dtype=torch.int64), lengths.cumsum(0))))
    for key in ('policy_mask', 'teacher_logits', 'teacher_values', 'seeds', 'episodes'):
        result[key] = fragment[key].cpu()[indices]
    return result


def distillation_errors(student_logits, student_values, teacher_logits, teacher_values, legal):
    assert legal.any(-1).all()
    student_log = student_logits.masked_fill(~legal, -torch.inf).log_softmax(-1).masked_fill(~legal, 0.)
    teacher_log = teacher_logits.masked_fill(~legal, -torch.inf).log_softmax(-1).masked_fill(~legal, 0.)
    probability = teacher_logits.masked_fill(~legal, -torch.inf).softmax(-1)
    kl = (probability * (teacher_log - student_log)).sum(-1)
    value_error = (student_values - teacher_values).square()
    assert torch.isfinite(kl).all() and torch.isfinite(value_error).all()
    return kl, value_error


def distill_inputs(fragment, indices, device):
    inputs = packed_inputs(fragment['data'].to(device), fragment['offsets'].to(device), indices, device)
    inputs['candidate_mask'] &= fragment['policy_mask'].to(device)[indices, :inputs['candidate_mask'].shape[1]]
    return inputs


@torch.no_grad()
def teacher_records(fragment, teacher, seeds, episodes, device):
    assert len(seeds) == len(fragment['actions'])
    fragment['data'] = fragment['data'][:int(fragment['offsets'][-1])]
    logits = torch.full((len(seeds), 128), -torch.inf, device=device)
    values = torch.empty(len(seeds), device=device)
    for indices in torch.arange(len(seeds), device=device).split(32):
        inputs = distill_inputs(fragment, indices, device)
        raw_logits, output_values = teacher(inputs)
        logits[indices, :raw_logits.shape[1]] = raw_logits
        values[indices] = output_values
    fragment.update(teacher_logits=logits, teacher_values=values,
                    seeds=torch.tensor(seeds, dtype=torch.int64, device=device),
                    episodes=torch.tensor(episodes, dtype=torch.int64, device=device))
    holdout = torch.tensor([held_out(seed) for seed in seeds], device=device)
    train_indices = (fragment['action_valid'] & ~holdout).nonzero().flatten()
    held_indices = (fragment['action_valid'] & holdout).nonzero().flatten()
    return compact_rows(fragment, train_indices), compact_rows(fragment, held_indices)


def minibatch_plan(count, world, rank, rng, device):
    quota = 8192 // world + int(rank < 8192 % world)
    permutation = torch.randperm(count, generator=rng, device=device)
    batches = list(permutation.split(quota))
    number = torch.tensor(len(batches), device=device)
    dist.all_reduce(number, op=dist.ReduceOp.MAX)
    return [batch.cpu() for batch in batches], int(number)


def distill_minibatch(model, optimizer, fragment, indices, device, world):
    indices = indices.to(device)
    amount = torch.tensor(len(indices), device=device, dtype=torch.int64)
    dist.all_reduce(amount)
    if int(amount) == 0:
        return
    chunks = list(indices.split(32)) if len(indices) else []
    iterations = torch.tensor(len(chunks), device=device)
    dist.all_reduce(iterations, op=dist.ReduceOp.MAX)
    optimizer.zero_grad(set_to_none=True)
    for i in range(int(iterations)):
        active = i < len(chunks)
        chunk = chunks[i] if active else torch.zeros(1, dtype=torch.int64, device=device)
        dummy = fragment if len(fragment['seeds']) else dummy_fragment(device)
        with model.no_sync() if i+1 < int(iterations) else nullcontext():
            current = fragment if active else dummy
            inputs = distill_inputs(current, chunk, device)
            logits, value = model(inputs)
            kl, error = distillation_errors(logits, value,
                current['teacher_logits'].to(device)[chunk, :logits.shape[1]],
                current['teacher_values'].to(device)[chunk], inputs['candidate_mask'])
            loss = (kl + .5 * error).sum() * (world / int(amount) if active else 0.)
            loss.backward()
    assert all(p.grad is None or torch.isfinite(p.grad).all() for p in model.parameters())
    optimizer.step()


def dummy_fragment(device):
    data = torch.zeros(35, dtype=torch.uint8, device=device)
    data[0], data[2], data[28] = 1, 1, 3
    return dict(data=data, offsets=torch.tensor([0, 35], device=device),
                policy_mask=torch.ones((1, 128), dtype=torch.bool, device=device),
                teacher_logits=torch.zeros((1, 128), device=device),
                teacher_values=torch.zeros(1, device=device), seeds=torch.zeros(1, device=device))


def freeze_holdout(state, completed, root, rank):
    eligible = {e['distill_episode_id'] for e in completed if held_out(e['seed'])}
    if not eligible:
        return False
    frozen = []
    for i, item in enumerate(state['heldout']):
        path = root / item['path']
        assert sha256(path) == item['sha256']
        fragment = torch.load(path, map_location='cpu', weights_only=False)
        selected = torch.tensor([j for j, identity in enumerate(fragment['episodes'].tolist())
                                 if identity in eligible], dtype=torch.int64)
        if not len(selected):
            continue
        path = root / f'data-rank-{rank}' / f'frozen-holdout-{i}.pt'
        atomic_checkpoint(path, compact_rows(fragment, selected))
        frozen.append(dict(path=str(path.relative_to(root)), sha256=sha256(path)))
    state['heldout'] = frozen
    state['heldout_frozen_episodes'] = sorted(eligible)
    state['heldout_frozen'] = True
    return True


@torch.no_grad()
def heldout_report(model, paths, root, device, deadline):
    totals = torch.zeros(3, dtype=torch.float64, device=device)
    expired = False
    for item in paths:
        path = root / item['path']
        assert sha256(path) == item['sha256']
        fragment = torch.load(path, map_location=device, weights_only=False)
        assert all(held_out(int(seed)) for seed in fragment['seeds'].tolist())
        for indices in torch.arange(len(fragment['seeds']), device=device).split(32):
            if time.monotonic() >= deadline:
                expired = True
                break
            inputs = distill_inputs(fragment, indices, device)
            logits, values = model(inputs)
            kl, error = distillation_errors(logits, values,
                fragment['teacher_logits'][indices, :logits.shape[1]], fragment['teacher_values'][indices],
                inputs['candidate_mask'])
            totals += torch.stack((kl.double().sum(), error.double().sum(), torch.tensor(len(indices), device=device)))
        if expired:
            break
    if collective_stop(expired, device):
        raise AllocationEnded()
    dist.all_reduce(totals)
    return dict(kl=float(totals[0]/totals[2]), value_error=float(totals[1]/totals[2]),
                decisions=int(totals[2])) if totals[2] else None


def rl_stop(assessment, best, best_floor, stagnant):
    if best_floor-assessment['average_floor'] > 1.:
        return best, best_floor, stagnant, 'average_floor_more_than_one_below_best'
    improved = any(assessment['reach'][key] - best[key] >= .02 for key in REACH)
    stagnant = 0 if improved else stagnant+1
    best = {key: max(best[key], assessment['reach'][key]) for key in REACH}
    return best, max(best_floor, assessment['average_floor']), stagnant, (
        'three_assessments_without_two_point_improvement' if stagnant >= 3 else None)


def search_state(search):
    return {key: deepcopy(getattr(search, key)) for key in (
        'metrics', 'capacity_metrics', 'loop_metrics', 'event_trial_max_steps',
        'searched_decisions', 'combat_diagnostics')}


def restore_search(search, saved):
    for key, value in saved.items():
        setattr(search, key, value)


def rng_state(device):
    return dict(torch=torch.get_rng_state(), cuda=torch.cuda.get_rng_state(device), python=random.getstate())


def restore_rng(state, device):
    torch.set_rng_state(state['torch'])
    torch.cuda.set_rng_state(state['cuda'], device)
    random.setstate(state['python'])


def charge_time(state, elapsed, world, distilling):
    state['node_seconds'] += elapsed*world/8
    if distilling:
        state['distill_node_seconds'] += elapsed*world/8
        state['report_seconds'] += elapsed


def stage_deadline(now, seconds, node_cap, node_used, world, parent_deadline):
    return min(now+seconds, parent_deadline, now+(node_cap-node_used)*8/world)-120


def rl_update(model, optimizer, fragment, rng, completed_updates, imitation_beta):
    lr = (0., 3.3e-5, 6.7e-5, 1e-4)[min(completed_updates, 3)]
    for group in optimizer.param_groups:
        group['lr'] = lr
    return dict(update(model, optimizer, fragment, rng, 4, imitation_beta), lr=lr)


def train_student(args, source, hashes, device, rank, world):
    assert args.node_seconds is not None and args.node_seconds > 0
    assert args.node_seconds <= (21600 if args.task == 'distill' else 30000)
    assert args.resume is None or args.resume.resolve() == args.output.resolve()
    commit = engine_commit()
    if args.task == 'distill':
        assert args.selection is not None
        selection_hash = sha256(args.selection)
        selection = json.loads(args.selection.read_text())
        assert selection['teacher_hashes'] == hashes and selection['passed']
        assert selection['config'] == select_benchmark(selection['rows'])
        config = ModelConfig(**selection['config'])
        student_hash = None
        student_source = None
    else:
        assert args.student is not None
        student_path = args.student / f'rank-{rank % 7}.pt'
        student_hash = sha256(student_path)
        student_source = torch.load(student_path, map_location='cpu', weights_only=False)
        assert student_source['task'] == 'distill' and student_source['teacher_hashes'] == hashes
        assert student_source['world_size'] == 7 and world in (7, 8)
        assert student_source['state']['phase'] == 'complete' and student_source['state']['distill_gate_passed']
        assert student_source['state']['assessments'][-1]['average_floor'] >= 19.
        config = ModelConfig(**student_source['config'])
        selection_hash = student_source['selection_sha256']
    assert asdict(config) in (asdict(LARGE), asdict(MEDIUM))
    torch.manual_seed(1729)
    policy = Policy(config).to(device)
    if student_source is not None:
        policy.load_state_dict(student_source['model'])
    ddp = DistributedDataParallel(policy, device_ids=[device.index], broadcast_buffers=False)
    optimizer = torch.optim.Adam(policy.parameters(), lr=3e-4 if args.task == 'distill' else 1e-4)
    ppo_rng = torch.Generator(device=device).manual_seed(1729+rank)
    collector = new_collector(device, rank)
    if args.task == 'distill':
        collector = DistillCollector(collector.search, collector.seed_rng, rank=rank)
    search = collector.search
    search.evidence_directory = args.output
    teacher = None
    if args.task == 'distill':
        teacher = Policy(ModelConfig()).to(device).eval().requires_grad_(False)
        teacher.load_state_dict(source['model'])
    torch.manual_seed(1729+rank)
    state = dict(phase='training', reason='initialized', trained_seconds=0., node_seconds=0.,
        distill_node_seconds=0., next_evaluation=1800., next_report=600., report_seconds=0.,
        reports=[], heldout=[], heldout_frozen=False, heldout_frozen_episodes=[],
        seen_train_episodes=set(), seen_heldout_episodes=set(),
        ready=None, ready_path=None, plan=None, minibatch=0,
        plan_length=0, updates=0, shard=0, assessments=[], best=BASELINE['reach'].copy(),
        best_floor=BASELINE['average_floor'], stagnant=0, nondecreases=0,
        imitation_beta=source['imitation_beta'], imitation_entropy_target=source['imitation_entropy_target'],
        train_state=None, training_search=None, training_rng=None, health=[], distill_gate_passed=False,
        peak_allocated=0, peak_reserved=0)
    saved = torch.load(args.resume / f'rank-{rank}.pt', map_location='cpu', weights_only=False) if args.resume else None
    if saved:
        assert saved['commit'] == commit and saved['task'] == args.task
        assert saved['teacher_hashes'] == hashes and saved['source_rank_mapping'] == [r % 7 for r in range(world)]
        assert saved['config'] == asdict(config) and saved['world_size'] == world
        assert saved['selection_sha256'] == selection_hash and saved['student_sha256'] == student_hash
        assert saved['node_seconds_cap'] == args.node_seconds
        policy.load_state_dict(saved['model'])
        optimizer.load_state_dict(saved['optimizer'])
        ppo_rng.set_state(saved['ppo_rng'])
        state = saved['state']
        if state['phase'] in ('evaluation', 'complete'):
            collector = SegmentCollector(search, collector.seed_rng, rank=rank)
        collector.restore(saved['collector'])
        restore_search(search, saved['search'])
        restore_rng(saved['rng'], device)
        if state['ready'] is not None:
            state['ready'] = {key: value.to(device) for key, value in state['ready'].items()}
        for item in state['heldout']:
            assert sha256(args.output / item['path']) == item['sha256']
    torch.cuda.reset_peak_memory_stats(device)
    allocation_start = time.monotonic()
    last_charge = allocation_start
    allocation_prior = state['node_seconds']
    deadline = stage_deadline(allocation_start, args.seconds, args.node_seconds, allocation_prior, world,
        float(os.environ.get('C76_ALLOCATION_DEADLINE_MONOTONIC', 'inf')))
    search.deadline = deadline

    def charge():
        nonlocal last_charge
        now = time.monotonic()
        elapsed = torch.tensor(now-last_charge, device=device, dtype=torch.float64)
        dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
        charge_time(state, float(elapsed), world, args.task == 'distill' and state['phase'] == 'training')
        last_charge = now

    def save(reason):
        charge()
        state['peak_allocated'] = max(state['peak_allocated'], torch.cuda.max_memory_allocated(device))
        state['peak_reserved'] = max(state['peak_reserved'], torch.cuda.max_memory_reserved(device))
        atomic_checkpoint(args.output / f'rank-{rank}.pt', dict(format=1, commit=commit,
            task=args.task, config=asdict(config), teacher_hashes=hashes,
            teacher_source_commit=TEACHER_COMMIT, source_rank_mapping=[r % 7 for r in range(world)],
            selection_sha256=selection_hash, student_sha256=student_hash,
            world_size=world, lanes=256, trials=32, epochs=4, initialization=1729,
            node_seconds_cap=args.node_seconds, distillation_training_node_seconds_cap=14400,
            state=state, model=policy.state_dict(),
            optimizer=optimizer.state_dict(), ppo_rng=ppo_rng.get_state(),
            collector=collector.state(), search=search_state(search), rng=rng_state(device)))
        dist.barrier()
        if rank == 0:
            atomic_json(args.output / 'status.json', dict(task=args.task, reason=reason,
                phase=state['phase'], config=asdict(config), world_size=world,
                trained_seconds=state['trained_seconds'], node_seconds=state['node_seconds'],
                distill_node_seconds=state['distill_node_seconds'], updates=state['updates'],
                total_stage_node_seconds_cap=args.node_seconds,
                distillation_training_node_seconds_cap=14400,
                reports=state['reports'], assessments=state['assessments'],
                best=state['best'], best_floor=state['best_floor'], stagnant=state['stagnant'],
                distill_gate_passed=state['distill_gate_passed'],
                peak_torch_allocated_bytes=state['peak_allocated'],
                peak_torch_reserved_bytes=state['peak_reserved'],
                accounting='provisional_process_wall_times_world_over_8_parent_finalizes_actual_allocation'))

    def can_continue():
        charge()
        if waiting(rank, args.output, state['updates']):
            save('yield_to_waiting_user')
            return False
        if collective_stop(time.monotonic() >= deadline, device):
            save('allocation_limit')
            return False
        return True

    def begin_evaluation():
        nonlocal collector
        charge()
        state['train_state'] = deepcopy(collector.state())
        state['training_search'] = search_state(search)
        state['training_rng'] = rng_state(device)
        collector = SegmentCollector(search, collector.seed_rng, rank=rank)
        search.rng.manual_seed(2729+rank)
        search.metrics = dict.fromkeys(SEARCH_METRICS, 0)
        search.capacity_metrics = dict.fromkeys(CAPACITY_METRICS, 0)
        search.loop_metrics = dict.fromkeys(LOOP_METRICS, 0)
        search.event_trial_max_steps = 0
        search.combat_diagnostics = []
        state['phase'] = 'evaluation'
        search.deadline = deadline

    save('started')
    while state['phase'] != 'complete':
        if not can_continue():
            return
        if state['phase'] == 'evaluation':
            save('before_evaluation')
            archive = args.output / f"evaluation-{state['updates']}"
            archive.mkdir(exist_ok=True)
            archived_path = archive / f'rank-{rank}.pt'
            if not archived_path.exists():
                shutil.copyfile(args.output / f'rank-{rank}.pt', archived_path)
            policy.eval()
            seeds = list(range(2000000000+rank, 2000001024, world))
            expired = False
            try:
                episodes = (collector.completed if collector.evaluation_waiting is None
                            and len(collector.completed) == len(seeds) else collector.evaluate(policy, seeds))
            except AllocationEnded:
                expired = True
                episodes = collector.completed
            if collective_stop(expired, device):
                save('allocation_limit_during_evaluation')
                return
            name = f"E4-C76-{args.task}-{state['updates']}-N32-D1024"
            atomic_json(args.output / f'{name}-rank-{rank}.json', episodes)
            atomic_json(args.output / f'{name}-diagnostics-{rank}.json', search_state(search))
            dist.barrier()
            assessment = None
            if rank == 0:
                all_episodes = sum([json.loads((args.output / f'{name}-rank-{r}.json').read_text())
                                    for r in range(world)], [])
                assert len(all_episodes) == 1024
                assert {e['seed'] for e in all_episodes} == set(range(2000000000, 2000001024))
                diagnostics = [json.loads((args.output / f'{name}-diagnostics-{r}.json').read_text())
                               for r in range(world)]
                search_summary = {key: sum(row['metrics'][key] for row in diagnostics)
                                  for key in SEARCH_METRICS[:2]}
                search_summary.update({key: max(row['metrics'][key] for row in diagnostics)
                                       for key in SEARCH_METRICS[2:]})
                search_summary.update({key: sum(row['capacity_metrics'][key] for row in diagnostics)
                                       for key in CAPACITY_METRICS})
                search_summary.update({key: sum(row['loop_metrics'][key] for row in diagnostics)
                                       for key in LOOP_METRICS})
                fraction = (search_summary['combat_played_cards_limit']/search_summary['combat_trials']
                            if search_summary['combat_trials'] else 0.)
                assessment = dict(summaries(all_episodes), round=name, version=state['updates'],
                    trials=32, dev_seed_count=1024, world_size=world, lanes=256,
                    trained_seconds=state['trained_seconds'], config=asdict(config),
                    imitation_beta=state['imitation_beta'], imitation_entropy_target=state['imitation_entropy_target'],
                    combat=combat_diagnostics_summary(sum([row['combat_diagnostics'] for row in diagnostics], [])),
                    noncombat=noncombat_summary(all_episodes),
                    combat_played_cards_limit_fraction=fraction, **search_summary)
                atomic_json(args.output / f'{name}-summary.json', assessment)
                atomic_json(args.output / f'{name}-audit.json', audit(all_episodes, name, state['updates'], commit))
                temporary = args.output / f'{name}-episodes.json.gz.tmp'
                with gzip.open(temporary, 'wt', encoding='utf-8') as stream:
                    json.dump(all_episodes, stream, separators=(',', ':'))
                temporary.replace(args.output / f'{name}-episodes.json.gz')
                print(json.dumps(dict(event='evaluation', **assessment)), flush=True)
            shared = [assessment]
            dist.broadcast_object_list(shared, src=0)
            assessment = shared[0]
            state['assessments'].append(assessment)
            if assessment['combat_played_cards_limit_fraction'] > .001:
                state['phase'] = 'complete'
                save('combat_played_cards_limit_above_one_per_thousand')
                return
            if args.task == 'distill':
                state['distill_gate_passed'] = assessment['average_floor'] >= 19.
                state['phase'] = 'complete'
                save('distill_gate_passed' if state['distill_gate_passed'] else 'distill_gate_failed')
                return
            state['best'], state['best_floor'], state['stagnant'], stop = rl_stop(
                assessment, state['best'], state['best_floor'], state['stagnant'])
            if stop:
                state['phase'] = 'complete'
                save(stop)
                return
            collector = SegmentCollector(search, collector.seed_rng, rank=rank)
            collector.restore(state['train_state'])
            restore_search(search, state['training_search'])
            restore_rng(state['training_rng'], device)
            state['next_evaluation'] = state['trained_seconds']+1800
            state['phase'] = 'training'
            save('assessment_complete')
            continue
        started = time.monotonic()
        expired = False
        if args.task == 'distill':
            charge()
            training_left = (14400-state['distill_node_seconds'])*8/world
            if training_left <= 60:
                begin_evaluation()
                save('distillation_training_subcap')
                continue
            search.deadline = min(deadline, time.monotonic()+training_left-30)
            policy.eval()
            if state['ready_path'] is None:
                try:
                    ready = collector.collect(teacher)
                except AllocationEnded:
                    expired = True
                if collective_stop(expired, device):
                    charge()
                    if not collective_stop(time.monotonic() >= deadline, device):
                        begin_evaluation()
                        save('distillation_training_subcap_during_collection')
                        continue
                    else:
                        save('allocation_limit_during_teacher_collection')
                        return
                train_rows, held_rows = teacher_records(ready, teacher, collector.decision_seeds,
                                                       collector.decision_episodes, device)
                collector.decision_seeds = []
                collector.decision_episodes = []
                del ready
                state['seen_train_episodes'].update(train_rows['episodes'].tolist())
                state['seen_heldout_episodes'].update(held_rows['episodes'].tolist())
                folder = args.output / f'data-rank-{rank}'
                folder.mkdir(exist_ok=True)
                path = folder / f"train-{state['shard']}.pt"
                atomic_checkpoint(path, train_rows)
                state['ready_path'] = dict(path=str(path.relative_to(args.output)), sha256=sha256(path))
                if len(held_rows['seeds']) and not state['heldout_frozen']:
                    heldpath = folder / f"heldout-{state['shard']}.pt"
                    atomic_checkpoint(heldpath, held_rows)
                    state['heldout'].append(dict(path=str(heldpath.relative_to(args.output)), sha256=sha256(heldpath)))
                state['plan'], state['plan_length'] = minibatch_plan(
                    len(train_rows['seeds']), world, rank, ppo_rng, device)
                state['minibatch'] = 0
                state['shard'] += 1
                save('teacher_records_ready')
            path = args.output / state['ready_path']['path']
            assert sha256(path) == state['ready_path']['sha256']
            fragment = torch.load(path, map_location=device, weights_only=False)
            assert all(not held_out(int(seed)) for seed in fragment['seeds'].tolist())
            ddp.train()
            while state['minibatch'] < state['plan_length']:
                if not can_continue():
                    return
                if (14400-state['distill_node_seconds'])*8/world <= 60:
                    begin_evaluation()
                    save('distillation_training_subcap_during_optimizer')
                    break
                i = state['minibatch']
                indices = state['plan'][i] if i < len(state['plan']) else torch.empty(0, dtype=torch.int64)
                distill_minibatch(ddp, optimizer, fragment, indices, device, world)
                state['minibatch'] += 1
            if state['phase'] == 'evaluation':
                continue
            state['ready_path'], state['plan'] = None, None
            state['updates'] += 1
            charge()
            if state['report_seconds'] >= state['next_report']:
                if not can_continue():
                    return
                policy.eval()
                if not state['heldout_frozen']:
                    freeze_holdout(state, collector.completed, args.output, rank)
                frozen = torch.tensor(int(state['heldout_frozen']), device=device)
                dist.all_reduce(frozen, op=dist.ReduceOp.MIN)
                if not int(frozen):
                    state['next_report'] = state['report_seconds']+600
                    save('waiting_for_completed_holdout_episodes')
                    path.unlink()
                    continue
                try:
                    report = heldout_report(policy, state['heldout'], args.output, device, search.deadline)
                except AllocationEnded:
                    charge()
                    if not collective_stop(time.monotonic() >= deadline, device):
                        begin_evaluation()
                        save('distillation_training_subcap_during_heldout_report')
                        continue
                    else:
                        save('allocation_limit_during_heldout_report')
                        return
                charge()
                state['next_report'] = state['report_seconds']+600
                if report is not None:
                    previous = state['reports'][-1]['kl'] if state['reports'] else float('inf')
                    state['nondecreases'] = state['nondecreases']+1 if report['kl'] >= previous else 0
                    counts = torch.tensor([len(state['seen_train_episodes']), len(state['seen_heldout_episodes']),
                                           len(state['heldout_frozen_episodes'])], device=device)
                    dist.all_reduce(counts)
                    report.update(seconds=state['report_seconds'], expected_holdout_fraction=.05,
                                  seen_train_episodes=int(counts[0]), seen_heldout_episodes=int(counts[1]),
                                  actual_holdout_episode_fraction=float(counts[1]/counts[:2].sum()),
                                  fixed_complete_holdout_episodes=int(counts[2]))
                    state['reports'].append(report)
                    if rank == 0:
                        atomic_json(args.output / 'heldout-reports.json', state['reports'])
                        print(json.dumps(dict(event='heldout_report', **report)), flush=True)
                    if report['kl'] < .02 or state['nondecreases'] >= 3:
                        begin_evaluation()
            if state['phase'] == 'training' and state['distill_node_seconds'] >= 14400:
                begin_evaluation()
            save('distill_update_complete')
            path.unlink()
            continue
        policy.eval()
        try:
            if state['ready'] is None:
                state['ready'] = collector.collect(policy)
        except AllocationEnded:
            expired = True
        if collective_stop(expired, device):
            save('allocation_limit_during_training')
            return
        ddp.train()
        metrics = rl_update(ddp, optimizer, state['ready'], ppo_rng, state['updates'], state['imitation_beta'])
        state['imitation_beta'] = update_beta(state['imitation_beta'], state['imitation_entropy_target'], metrics['H_imit'])
        health = state['ready']['health'].clone()
        dist.all_reduce(health[:3])
        state['health'].append((int(health[0]), int(health[2])))
        state['health'] = state['health'][-20:]
        state['ready'] = None
        state['updates'] += 1
        elapsed = torch.tensor(time.monotonic()-started, device=device)
        dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
        state['trained_seconds'] += float(elapsed)
        if rank == 0:
            print(json.dumps(dict(event='rl_update', update=state['updates'],
                trained_seconds=state['trained_seconds'], **metrics)), flush=True)
        completed = sum(row[0] for row in state['health'])
        stalled = sum(row[1] for row in state['health'])
        if (completed == 0 and len(state['health']) >= 20) or stalled*20 > completed:
            state['phase'] = 'complete'
            save('training_health_stop')
            return
        if state['trained_seconds'] >= state['next_evaluation']:
            begin_evaluation()
        save('rl_update_complete')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('task', choices=('benchmark', 'distill', 'rl'))
    parser.add_argument('--teacher', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, required=True,
                        help='Current allocation wall seconds, at most 1200 for 4a or 14400 for 4b/4c')
    parser.add_argument('--node-seconds', type=float,
                        help='Frozen total stage eight-card-node-second cap, including evaluation; '
                             'resume subtracts saved usage. Distillation training has a separate 14400 subcap')
    parser.add_argument('--resume', type=Path)
    parser.add_argument('--selection', type=Path)
    parser.add_argument('--student', type=Path)
    args = parser.parse_args()
    torch.set_num_threads(1)
    rank, world = int(os.environ['RANK']), int(os.environ['WORLD_SIZE'])
    assert world == 1 if args.task == 'benchmark' else world in (7, 8)
    assert 0 < args.seconds <= (1200 if args.task == 'benchmark' else 14400)
    device = torch.device('cuda', int(os.environ['LOCAL_RANK']))
    torch.cuda.set_device(device)
    dist.init_process_group('nccl', timeout=timedelta(minutes=60))
    args.output.mkdir(parents=True, exist_ok=True)
    source, hashes = teacher_source(args.teacher, rank)
    try:
        if args.task == 'benchmark':
            benchmark(args, source, hashes, device)
        else:
            train_student(args, source, hashes, device, rank, world)
    finally:
        dist.destroy_process_group()


if __name__ == '__main__':
    main()
