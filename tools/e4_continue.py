import argparse
from collections import Counter, deque
from copy import deepcopy
from datetime import datetime, timedelta
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
import sys
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.checkpoint import engine_commit
from train.e4 import AllocationEnded, CAPACITY_METRICS, REACH, SEARCH_METRICS, RoomSearch, SegmentCollector, combat_diagnostics_summary
from train.e4_learner import update, update_beta
from train.e4_rewards import noncombat_summary
from train.model import ModelConfig, Policy
from tools.e4_run import atomic_checkpoint, atomic_json, audit, collective_stop, summaries


def queue_snapshot():
    jobs = []
    for line in subprocess.check_output(['scontrol', 'show', 'jobs', '-o'], text=True, timeout=60).splitlines():
        fields = dict(token.split('=', 1) for token in line.split() if '=' in token)
        if any(fields.get(key) is None for key in (
                'JobId', 'JobName', 'UserId', 'JobState', 'Reason', 'Dependency',
                'TimeLimit', 'StartTime', 'RunTime', 'NodeList', 'TRES')):
            raise ValueError(f'Incomplete scontrol job fields: {line}')
        jobs.append({key: fields.get(key) for key in (
            'JobId', 'JobName', 'UserId', 'JobState', 'Reason', 'Dependency',
            'TimeLimit', 'StartTime', 'RunTime', 'NodeList', 'TresPerNode', 'TRES')})
    raw_node = subprocess.check_output(['scontrol', 'show', 'node', 'gpu01', '-o', '-d'], text=True, timeout=60)
    fields = dict(token.split('=', 1) for token in raw_node.split() if '=' in token)
    node = {key: fields.get(key) for key in ('NodeName', 'State', 'Gres', 'GresUsed', 'CfgTRES', 'AllocTRES')}
    if any(value is None for value in node.values()):
        raise ValueError(f'Incomplete scontrol node fields: {raw_node}')
    blockers = []
    for job in jobs:
        if job.get('UserId').split('(')[0] == os.environ['USER']:
            continue
        dependency_wait = job.get('Reason') in ('Dependency', 'DependencyNeverSatisfied')
        if job.get('JobState') == 'PENDING' and not dependency_wait:
            blockers.append(job.get('JobId'))
    return dict(checked_at=datetime.now().astimezone().isoformat(), jobs=jobs, node=node,
                blocking_job_ids=blockers,
                source='scontrol show jobs -o; scontrol show node gpu01 -o -d')


def waiting(rank, output, version):
    result = [None]
    if rank == 0:
        try:
            snapshot = queue_snapshot()
            atomic_json(output / 'queue-last.json', snapshot)
            result[0] = bool(snapshot.get('blocking_job_ids'))
            if result[0]:
                atomic_json(output / f'queue-yield-{version}.json', snapshot)
        except Exception as error:
            result[0] = True
            evidence = dict(checked_at=datetime.now().astimezone().isoformat(),
                            version=version, error_type=type(error).__name__, error=str(error))
            try:
                atomic_json(output / f'queue-error-{version}.json', evidence)
            except OSError as write_error:
                print(json.dumps(dict(evidence, evidence_write_error=str(write_error))),
                      file=sys.stderr, flush=True)
    dist.broadcast_object_list(result, src=0)
    return result[0]


def main(case):
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--resume', type=Path)
    parser.add_argument('--seconds', type=int, required=True)
    parser.add_argument('--evaluate-only', action='store_true')
    if case == 'B':
        parser.add_argument('--training-seconds', type=float, required=True)
    args = parser.parse_args()
    rank, world = int(os.environ['RANK']), int(os.environ['WORLD_SIZE'])
    assert world in (7, 8)
    torch.set_num_threads(1)
    device = torch.device('cuda', int(os.environ['LOCAL_RANK']))
    torch.cuda.set_device(device)
    dist.init_process_group('nccl', timeout=timedelta(minutes=60))
    args.output.mkdir(parents=True, exist_ok=True)
    commit = engine_commit()
    source_path = args.source / f'rank-{rank}.pt'
    source_hash = hashlib.sha256(source_path.read_bytes()).hexdigest()
    source = torch.load(source_path, map_location='cpu', weights_only=False)
    assert case in ('A', 'B')
    if case == 'A':
        assert source['commit'] == '3b54843df8c36d856f9c9425bb1279833f956fab'
        assert source['initialization'] == 1729 and source['version'] == 105
    else:
        assert source['commit'] == 'd28c060f92e38385670d70defad5a41fea3421a9'
        assert source['initialization'] == 1729 and source['version'] == 163
        assert world == source['world_size'] == 7
    assert source['reason'] == 'before_evaluation'
    assert source['lanes'] == 256 and source['trials'] == 32 and source['phase'] == 'evaluation'
    assert source['ready'] is None and source['train_state']['partial'] is None
    source_version = source['version']
    prefix = 'E4-C53' if case == 'A' else 'E4-C68-B'
    torch.manual_seed(1729)
    policy = Policy(ModelConfig()).to(device)
    ddp = DistributedDataParallel(policy, device_ids=[device.index], broadcast_buffers=False)
    optimizer = torch.optim.Adam(policy.parameters(), lr=3e-4)
    ppo_rng = torch.Generator(device=device)
    search_rng = torch.Generator(device=device)
    seed_rng = torch.Generator()
    fork_rng = torch.Generator().manual_seed(681729 + rank) if case == 'B' else None
    deadline = time.monotonic() + args.seconds - 120
    phase, epochs = 'training', 4
    trained_seconds, evaluation_seconds, next_evaluation = 0., 0., 1800.
    if case == 'B':
        assert args.training_seconds > 0
        next_evaluation = args.training_seconds
    updates, actual_steps = [], 0
    health_history = deque(source['health_history'], maxlen=20)
    train_state = deepcopy(source['train_state'])
    training_rng = source['training_rng']
    evaluation_state, ready = None, None
    baseline_path = (args.source.parent / 'E4-C49-105-N32-D1024-summary.json' if case == 'A'
                     else args.source.parent / f'E4-C53-{source_version}-N32-D1024-summary.json')
    baseline = json.loads(baseline_path.read_text())
    assert baseline['version'] == source_version
    if case == 'A':
        assert baseline['average_floor'] == 15.2451171875
    else:
        train_state['fork_rng'] = fork_rng.get_state()
    assessments = [baseline]
    best, stagnant = baseline['reach'].copy(), 0
    best_floor = baseline['average_floor']
    imitation_beta = source['imitation_beta']
    imitation_entropy_target = source['imitation_entropy_target']
    saved = torch.load(args.resume / f'rank-{rank}.pt', map_location='cpu', weights_only=False) if args.resume else None
    if args.evaluate_only:
        assert saved is not None and saved['ready'] is None
    if saved:
        assert saved['commit'] == commit and saved['source_sha256'] == source_hash
        if case == 'B':
            assert saved['case'] == 'B' and saved['target_training_seconds'] == args.training_seconds
        phase, epochs = saved['phase'], saved['epochs']
        assert epochs == 4
        best_floor = saved['best_floor']
        imitation_beta = saved['imitation_beta']
        imitation_entropy_target = saved['imitation_entropy_target']
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
    collector = SegmentCollector(search, seed_rng, rank=rank, fork_rng=fork_rng)
    if saved is None and case == 'A':
        start = time.monotonic()
        slots = [s for s, seed in enumerate(train_state['seeds']) if seed is not None]
        search.batch.reset_to_many(slots, [train_state['seeds'][s] for s in slots], [[] for _ in slots])
        search.check(slots)
        search.reset_seen(slots)
        for s in slots:
            search.provenance[s] = (train_state['seeds'][s], [])
        for t in range(max(len(train_state['paths'][s]) for s in slots)):
            active = [s for s in slots if t < len(train_state['paths'][s])]
            search.step(active, [train_state['paths'][s][t] for s in active])
        train_state['reward_history'] = deepcopy(search.reward_history[:search.lanes])
        for history in train_state['reward_history']:
            history.counts.clear()
        atomic_json(args.output / f'reward-history-migration-{rank}.json', dict(
            source_sha256=source_hash, source_version=source_version, lanes=len(slots),
            decisions=sum(len(train_state['paths'][s]) for s in slots),
            seconds=time.monotonic()-start, method='replay saved actual prefixes using visible observations'))
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
        search.capacity_metrics = saved['capacity_metrics']
        search.event_trial_max_steps = saved['event_trial_max_steps']
        search.loop_metrics = saved['loop_metrics']
        search.combat_diagnostics = saved['combat_diagnostics']
        if case == 'B':
            search.fork_metrics = saved['fork_metrics']

    def checkpoint(reason):
        nonlocal train_state, evaluation_state
        if phase == 'training':
            train_state = collector.state()
        elif phase == 'evaluation':
            evaluation_state = collector.state()
        atomic_checkpoint(args.output / f'rank-{rank}.pt', dict(commit=commit,
            source_sha256=source_hash, source_commit=source['commit'], source_version=source_version, case=case,
            fork_metrics=search.fork_metrics,
            target_training_seconds=args.training_seconds if case == 'B' else None,
            initialization=1729, phase=phase, reason=reason, lanes=256, trials=32, world_size=world,
            version=source_version+len(updates), epochs=epochs, trained_seconds=trained_seconds,
            source_trained_seconds=source['trained_seconds'], next_evaluation=next_evaluation,
            evaluation_seconds=evaluation_seconds, updates=updates, actual_steps=actual_steps,
            model=policy.state_dict(), optimizer=optimizer.state_dict(), ppo_rng=ppo_rng.get_state(),
            torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device),
            training_rng=training_rng, train_state=train_state, evaluation_state=evaluation_state,
            ready=ready, health_history=list(health_history), assessments=assessments,
            best=best, stagnant=stagnant, search_metrics=search.metrics,
            capacity_metrics=search.capacity_metrics, combat_diagnostics=search.combat_diagnostics,
            best_floor=best_floor, imitation_beta=imitation_beta,
            imitation_entropy_target=imitation_entropy_target,
            event_trial_max_steps=search.event_trial_max_steps, loop_metrics=search.loop_metrics))
        dist.barrier()
        if rank == 0:
            atomic_json(args.output / 'status.json', dict(reason=reason, phase=phase,
                job=os.environ['SLURM_JOB_ID'], commit=commit, source_directory=str(args.source),
                source_version=source_version, version=source_version+len(updates), case=case, lanes=256, epochs=epochs, trials=32, world_size=world,
                evaluation_lanes=256, dev_seed_count=1024, trained_seconds=trained_seconds,
                next_evaluation=next_evaluation, evaluation_seconds=evaluation_seconds,
                capacity_metrics=search.capacity_metrics,
                updates=updates, actual_steps=actual_steps, assessments=assessments,
                best=best, stagnant=stagnant, best_floor=best_floor,
                imitation_beta=imitation_beta, imitation_entropy_target=imitation_entropy_target))

    try:
        if phase == 'complete':
            return
        checkpoint('started')
        while True:
            if waiting(rank, args.output, source_version+len(updates)):
                checkpoint('yield_to_waiting_user')
                return
            if collective_stop(time.monotonic() >= deadline, device):
                checkpoint('allocation_limit')
                return
            if phase == 'training':
                if args.evaluate_only:
                    next_evaluation = trained_seconds
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
                    metrics = update(ddp, optimizer, ready, ppo_rng, epochs, imitation_beta)
                    if imitation_entropy_target is None:
                        imitation_entropy_target = metrics['H_imit']
                    imitation_beta = update_beta(imitation_beta, imitation_entropy_target, metrics['H_imit'])
                    metrics.update(imitation_beta=imitation_beta, imitation_entropy_target=imitation_entropy_target)
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
                    row = dict(event='update', update=len(updates)+1, version=source['version']+len(updates)+1,
                        training_seconds=trained_seconds, update_seconds=float(elapsed),
                        actual_steps=int(steps[0]), sl_trial_steps=int(steps[1]), lanes=256, world_size=world,
                        next_epochs=epochs, completed_episodes=completed, stalled_episodes=stalled,
                        completed_average_floor=floor_sum/completed if completed else None,
                        active_max_floor_steps=max_steps, stall_classes=dict(stall_classes), **metrics)
                    updates.append(row)
                    ready = None
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
                collector = SegmentCollector(search, seed_rng, rank=rank, fork_rng=fork_rng)
                search_rng.manual_seed(2729+rank)
                search.metrics = dict.fromkeys(SEARCH_METRICS, 0)
                search.capacity_metrics = dict.fromkeys(CAPACITY_METRICS, 0)
                search.event_trial_max_steps = 0
                search.combat_diagnostics = []
                phase = 'evaluation'
            if waiting(rank, args.output, source_version+len(updates)):
                checkpoint('yield_to_waiting_user')
                return
            checkpoint('before_evaluation')
            evaluation_checkpoint = args.output / f'evaluation-{source_version+len(updates)}'
            evaluation_checkpoint.mkdir(exist_ok=True)
            archived_rank = evaluation_checkpoint / f'rank-{rank}.pt'
            if not archived_rank.exists():
                shutil.copyfile(args.output / f'rank-{rank}.pt', archived_rank)
            dist.barrier()
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
            capacity_counts = torch.tensor([search.capacity_metrics[k] for k in CAPACITY_METRICS], device=device)
            dist.all_reduce(capacity_counts, op=dist.ReduceOp.SUM)
            search_summary.update(zip(CAPACITY_METRICS, capacity_counts.cpu().tolist()))
            search_summary['combat_played_cards_limit_fraction'] = (
                search_summary['combat_played_cards_limit'] / search_summary['combat_trials']
                if search_summary['combat_trials'] else 0.)
            version = source_version+len(updates)
            round_name = f'{prefix}-{version}-N32-D1024'
            atomic_json(args.output / f'{round_name}-rank-{rank}.json', episodes)
            atomic_json(args.output / f'{round_name}-combat-{rank}.json', search.combat_diagnostics)
            atomic_json(args.output / f'{round_name}-fork-{rank}.json', search.fork_metrics)
            dist.barrier()
            assessment = None
            if rank == 0:
                all_episodes = sum([json.loads((args.output / f'{round_name}-rank-{r}.json').read_text())
                                    for r in range(world)], [])
                assert len(all_episodes) == 1024
                assert {e['seed'] for e in all_episodes} == set(range(2000000000, 2000001024))
                combat_rows = sum([json.loads((args.output / f'{round_name}-combat-{r}.json').read_text())
                                   for r in range(world)], [])
                fork_rows = [json.loads((args.output / f'{round_name}-fork-{r}.json').read_text()) for r in range(world)]
                fork_totals = {key: (max(row[key] for row in fork_rows) if key in ('score_gap_max',)
                                    or key.endswith('max_steps') else sum(row[key] for row in fork_rows))
                               for key in search.fork_metrics}
                fork_totals['time_fraction'] = fork_totals['seconds'] / (world * trained_seconds) if trained_seconds else 0.
                fork_totals['score_gap_mean'] = (fork_totals['score_gap_sum'] / fork_totals['score_gap_count']
                                                 if fork_totals['score_gap_count'] else None)
                fork_totals['combat_played_cards_limit_fraction'] = (fork_totals['combat_played_cards_limit']
                    / fork_totals['combat_trials'] if fork_totals['combat_trials'] else 0.)
                assessment = dict(summaries(all_episodes), round=round_name, version=version,
                    case=case, source_version=source_version, fork=fork_totals,
                    source_trained_seconds=source['trained_seconds'],
                    target_training_seconds=args.training_seconds if case == 'B' else None,
                    training_seconds_overshoot=max(0., trained_seconds-args.training_seconds) if case == 'B' else None,
                    trials=32, training_lanes=256, evaluation_lanes=256, dev_seed_count=1024, world_size=world,
                    evaluation_seconds=evaluation_seconds, trained_seconds=trained_seconds,
                    update_count=len(updates), actual_steps=actual_steps, next_epochs=epochs,
                    combat=combat_diagnostics_summary(combat_rows), noncombat=noncombat_summary(all_episodes),
                    imitation_beta=imitation_beta,
                    imitation_entropy_target=imitation_entropy_target, best_average_floor_before=best_floor,
                    **search_summary)
                atomic_json(args.output / f'{round_name}-summary.json', assessment)
                atomic_json(args.output / f'{round_name}-audit.json', audit(all_episodes, round_name, version, commit))
                print(json.dumps(dict(event='evaluation', **assessment)), flush=True)
            shared = [assessment]
            dist.broadcast_object_list(shared, src=0)
            assessment = shared[0]
            assessments.append(assessment)
            if (assessment['combat_played_cards_limit_fraction'] > .001
                    or assessment['fork']['combat_played_cards_limit_fraction'] > .001):
                phase = 'complete'
                checkpoint('combat_played_cards_limit_above_one_per_thousand')
                return
            if args.evaluate_only:
                phase = 'complete'
                checkpoint('evaluation_only_complete')
                return
            if case == 'B' and trained_seconds >= args.training_seconds:
                phase = 'complete'
                checkpoint('matched_training_seconds_complete')
                return
            if best_floor - assessment['average_floor'] > 1.:
                phase = 'complete'
                checkpoint('average_floor_more_than_one_below_best')
                return
            best_floor = max(best_floor, assessment['average_floor'])
            improved = any(assessment['reach'][key]-best[key] >= .02 for key in REACH)
            stagnant = 0 if improved else stagnant+1
            best = {key: max(best[key], assessment['reach'][key]) for key in REACH}
            collector = SegmentCollector(search, seed_rng, rank=rank, fork_rng=fork_rng)
            collector.restore(train_state)
            torch.set_rng_state(training_rng['torch_rng'])
            torch.cuda.set_rng_state(training_rng['cuda_rng'], device)
            evaluation_state, evaluation_seconds = None, 0.
            next_evaluation = (args.training_seconds if case == 'B'
                               else trained_seconds+1800)
            search.combat_diagnostics = None
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
                combat_diagnostics=search.combat_diagnostics, imitation_beta=imitation_beta,
                imitation_entropy_target=imitation_entropy_target,
                model=policy.state_dict(), optimizer=optimizer.state_dict(), ppo_rng=ppo_rng.get_state(),
                torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device)))
        dist.destroy_process_group()


if __name__ == '__main__':
    main('A')
