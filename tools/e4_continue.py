import argparse
from collections import Counter, deque
from copy import deepcopy
from datetime import timedelta
import hashlib
import json
import os
from pathlib import Path
import sys
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.checkpoint import engine_commit
from train.e4 import AllocationEnded, REACH, SEARCH_METRICS, RoomSearch, SegmentCollector
from train.e4_learner import update
from train.model import ModelConfig, Policy
from tools.e4_run import atomic_checkpoint, atomic_json, audit, collective_stop, queue_waiting, summaries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--resume', type=Path)
    parser.add_argument('--seconds', type=int, required=True)
    args = parser.parse_args()
    rank, world = int(os.environ['RANK']), int(os.environ['WORLD_SIZE'])
    assert world == 8
    torch.set_num_threads(1)
    device = torch.device('cuda', int(os.environ['LOCAL_RANK']))
    torch.cuda.set_device(device)
    dist.init_process_group('nccl', timeout=timedelta(minutes=60))
    args.output.mkdir(parents=True, exist_ok=True)
    commit = engine_commit()
    source_path = args.source / f'rank-{rank}.pt'
    source_hash = hashlib.sha256(source_path.read_bytes()).hexdigest()
    source = torch.load(source_path, map_location='cpu', weights_only=False)
    assert source['commit'] == 'd9539cc5805efe77e76031eb1a83a1fd69b4784b'
    assert source['initialization'] == 1729 and source['version'] == 97 and source['case'] == 'B'
    assert source['lanes'] == 256 and source['trials'] == 32 and source['phase'] == 'complete'
    assert source['ready'] is None and source['train_state']['partial'] is None
    torch.manual_seed(1729)
    policy = Policy(ModelConfig()).to(device)
    ddp = DistributedDataParallel(policy, device_ids=[device.index], broadcast_buffers=False)
    optimizer = torch.optim.Adam(policy.parameters(), lr=3e-4)
    ppo_rng = torch.Generator(device=device)
    search_rng = torch.Generator(device=device)
    seed_rng = torch.Generator()
    deadline = time.monotonic() + args.seconds - 120
    phase, epochs = 'training', 4
    trained_seconds, evaluation_seconds, next_evaluation = 0., 0., 3600.
    updates, actual_steps = [], 0
    health_history = deque(source['health_history'], maxlen=20)
    train_state = deepcopy(source['train_state'])
    training_rng = source['training_rng']
    evaluation_state, ready = None, None
    assessments = [source['assessment']]
    best, stagnant = source['assessment']['reach'].copy(), 0
    saved = torch.load(args.resume / f'rank-{rank}.pt', map_location='cpu', weights_only=False) if args.resume else None
    if saved:
        assert saved['commit'] == commit and saved['source_sha256'] == source_hash
        phase, epochs = saved['phase'], saved['epochs']
        trained_seconds, evaluation_seconds = saved['trained_seconds'], saved['evaluation_seconds']
        next_evaluation = saved['next_evaluation']
        updates, actual_steps = saved['updates'], saved['actual_steps']
        health_history = deque(saved['health_history'], maxlen=20)
        train_state, evaluation_state = saved['train_state'], saved['evaluation_state']
        training_rng, ready = saved['training_rng'], saved['ready']
        assessments, best, stagnant = saved['assessments'], saved['best'], saved['stagnant']
        if ready is not None:
            ready = {k: v.to(device) for k, v in ready.items()}
    weights = saved if saved else source
    policy.load_state_dict(weights['model'])
    optimizer.load_state_dict(weights['optimizer'])
    ppo_rng.set_state(weights['ppo_rng'])
    search = RoomSearch(256, 32, device, search_rng, scratch_slots=256*33)
    search.evidence_directory = args.output
    search.deadline = deadline
    collector = SegmentCollector(search, seed_rng, rank=rank)
    if phase == 'training':
        collector.restore(train_state)
    elif evaluation_state is not None:
        collector.restore(evaluation_state)
    else:
        search_rng.manual_seed(2729 + rank)
    rng = saved if saved else training_rng
    torch.set_rng_state(rng['torch_rng'])
    torch.cuda.set_rng_state(rng['cuda_rng'], device)
    if saved:
        search.metrics = saved['search_metrics']
        search.event_trial_max_steps = saved['event_trial_max_steps']
        search.loop_metrics = saved['loop_metrics']

    def checkpoint(reason):
        nonlocal train_state, evaluation_state
        if phase == 'training':
            train_state = collector.state()
        elif phase == 'evaluation':
            evaluation_state = collector.state()
        atomic_checkpoint(args.output / f'rank-{rank}.pt', dict(commit=commit,
            source_sha256=source_hash, source_commit=source['commit'], source_version=97,
            initialization=1729, phase=phase, reason=reason, lanes=256, trials=32,
            version=97+len(updates), epochs=epochs, trained_seconds=trained_seconds,
            source_trained_seconds=source['trained_seconds'], next_evaluation=next_evaluation,
            evaluation_seconds=evaluation_seconds, updates=updates, actual_steps=actual_steps,
            model=policy.state_dict(), optimizer=optimizer.state_dict(), ppo_rng=ppo_rng.get_state(),
            torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device),
            training_rng=training_rng, train_state=train_state, evaluation_state=evaluation_state,
            ready=ready, health_history=list(health_history), assessments=assessments,
            best=best, stagnant=stagnant, search_metrics=search.metrics,
            event_trial_max_steps=search.event_trial_max_steps, loop_metrics=search.loop_metrics))
        dist.barrier()
        if rank == 0:
            atomic_json(args.output / 'status.json', dict(reason=reason, phase=phase,
                job=os.environ['SLURM_JOB_ID'], commit=commit, source_directory=str(args.source),
                source_version=97, version=97+len(updates), lanes=256, epochs=epochs, trials=32,
                evaluation_lanes=256, dev_seed_count=1024, trained_seconds=trained_seconds,
                next_evaluation=next_evaluation, evaluation_seconds=evaluation_seconds,
                updates=updates, actual_steps=actual_steps, assessments=assessments,
                best=best, stagnant=stagnant))

    def waiting():
        rows = [queue_waiting() if rank == 0 else None]
        dist.broadcast_object_list(rows, src=0)
        return bool(rows[0])

    try:
        if phase == 'complete':
            return
        checkpoint('started')
        while True:
            if waiting():
                checkpoint('yield_to_waiting_user')
                return
            if collective_stop(time.monotonic() >= deadline, device):
                checkpoint('allocation_limit')
                return
            if phase == 'training':
                if trained_seconds < next_evaluation:
                    before, sl_before = collector.decisions, search.searched_decisions
                    start = time.monotonic()
                    policy.eval()
                    expired = False
                    try:
                        if ready is None:
                            ready = collector.collect(policy)
                    except AllocationEnded:
                        expired = True
                    if collective_stop(expired, device):
                        checkpoint('allocation_limit')
                        return
                    ddp.train()
                    metrics = update(ddp, optimizer, ready, ppo_rng, epochs)
                    health = ready['health'].clone()
                    dist.all_reduce(health[:3], op=dist.ReduceOp.SUM)
                    dist.all_reduce(health[3:], op=dist.ReduceOp.MAX)
                    completed, floor_sum, stalled, max_steps = health.cpu().tolist()
                    classes = [None] * world
                    dist.all_gather_object(classes, collector.last_stall_classes)
                    stall_classes = Counter()
                    for counts in classes:
                        stall_classes.update(counts)
                    health_history.append((completed, stalled, dict(stall_classes)))
                    elapsed = torch.tensor(time.monotonic()-start, device=device)
                    dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
                    steps = torch.tensor([collector.decisions-before, search.searched_decisions-sl_before], device=device)
                    dist.all_reduce(steps, op=dist.ReduceOp.SUM)
                    trained_seconds += float(elapsed)
                    actual_steps += int(steps[0])
                    next_epochs = min(8, epochs+2) if metrics['kl'] < .005 else max(2, epochs//2) if metrics['kl'] > .02 else epochs
                    row = dict(event='update', update=len(updates)+1, version=98+len(updates),
                        training_seconds=trained_seconds, update_seconds=float(elapsed),
                        actual_steps=int(steps[0]), sl_trial_steps=int(steps[1]), lanes=256,
                        next_epochs=next_epochs, completed_episodes=completed, stalled_episodes=stalled,
                        completed_average_floor=floor_sum/completed if completed else None,
                        active_max_floor_steps=max_steps, stall_classes=dict(stall_classes), **metrics)
                    updates.append(row)
                    epochs, ready = next_epochs, None
                    if rank == 0:
                        print(json.dumps(row), flush=True)
                    total_completed = sum(x[0] for x in health_history)
                    total_stalled = sum(x[1] for x in health_history)
                    if total_completed == 0 or total_stalled*20 > total_completed:
                        checkpoint('training_health_stop')
                        return
                    checkpoint('update_complete')
                    if trained_seconds < next_evaluation:
                        continue
                train_state = deepcopy(collector.state())
                training_rng = dict(torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device))
                collector = SegmentCollector(search, seed_rng, rank=rank)
                search_rng.manual_seed(2729+rank)
                search.metrics = dict.fromkeys(SEARCH_METRICS, 0)
                search.event_trial_max_steps = 0
                phase = 'evaluation'
            if waiting():
                checkpoint('yield_to_waiting_user')
                return
            checkpoint('before_evaluation')
            policy.eval()
            start = time.monotonic()
            expired = False
            seeds = list(range(2000000000+rank, 2000001024, world))
            try:
                episodes = (collector.completed if collector.evaluation_waiting is None
                            and len(collector.completed) == len(seeds) else collector.evaluate(policy, seeds))
            except AllocationEnded:
                episodes = collector.completed
                expired = True
            elapsed = torch.tensor(time.monotonic()-start, device=device)
            dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
            evaluation_seconds += float(elapsed)
            if collective_stop(expired, device):
                atomic_json(args.output / f'partial-evaluation-{rank}.json', episodes)
                checkpoint('allocation_limit_during_evaluation')
                return
            counts = torch.tensor([search.metrics[k] for k in SEARCH_METRICS[:2]], device=device)
            maxima = torch.tensor([search.metrics[k] for k in SEARCH_METRICS[2:]], device=device)
            dist.all_reduce(counts, op=dist.ReduceOp.SUM)
            dist.all_reduce(maxima, op=dist.ReduceOp.MAX)
            search_summary = dict(zip(SEARCH_METRICS, counts.cpu().tolist()+maxima.cpu().tolist()))
            version = 97+len(updates)
            round_name = f'E4-C39-{version}-N32-D1024'
            atomic_json(args.output / f'{round_name}-rank-{rank}.json', episodes)
            dist.barrier()
            assessment = None
            if rank == 0:
                all_episodes = sum([json.loads((args.output / f'{round_name}-rank-{r}.json').read_text())
                                    for r in range(world)], [])
                assert len(all_episodes) == 1024
                assert {e['seed'] for e in all_episodes} == set(range(2000000000, 2000001024))
                assessment = dict(summaries(all_episodes), round=round_name, version=version,
                    trials=32, training_lanes=256, evaluation_lanes=256, dev_seed_count=1024,
                    evaluation_seconds=evaluation_seconds, trained_seconds=trained_seconds,
                    update_count=len(updates), actual_steps=actual_steps, next_epochs=epochs, **search_summary)
                atomic_json(args.output / f'{round_name}-summary.json', assessment)
                atomic_json(args.output / f'{round_name}-audit.json', audit(all_episodes, round_name, version, commit))
                print(json.dumps(dict(event='evaluation', **assessment)), flush=True)
            shared = [assessment]
            dist.broadcast_object_list(shared, src=0)
            assessment = shared[0]
            assessments.append(assessment)
            improved = any(assessment['reach'][key]-best[key] >= .02 for key in REACH)
            stagnant = 0 if improved else stagnant+1
            best = {key: max(best[key], assessment['reach'][key]) for key in REACH}
            collector = SegmentCollector(search, seed_rng, rank=rank)
            collector.restore(train_state)
            torch.set_rng_state(training_rng['torch_rng'])
            torch.cuda.set_rng_state(training_rng['cuda_rng'], device)
            evaluation_state, evaluation_seconds = None, 0.
            next_evaluation = trained_seconds+3600
            phase = 'training'
            if stagnant >= 3:
                phase = 'complete'
                checkpoint('three_assessments_without_two_point_improvement')
                return
            checkpoint('assessment_complete')
    finally:
        if sys.exc_info()[0] is not None:
            atomic_checkpoint(args.output / f'fault-rank-{rank}.pt', dict(commit=commit,
                phase=phase, collector=collector.state(), train_state=train_state, ready=ready,
                model=policy.state_dict(), optimizer=optimizer.state_dict(), ppo_rng=ppo_rng.get_state(),
                torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device)))
        dist.destroy_process_group()


if __name__ == '__main__':
    main()
