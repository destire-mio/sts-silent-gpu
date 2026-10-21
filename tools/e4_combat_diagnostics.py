import argparse
from datetime import timedelta
import hashlib
import json
import os
from pathlib import Path
import sys
import time

import torch
import torch.distributed as dist

from train.checkpoint import engine_commit
from train.e4 import AllocationEnded, RoomSearch, SegmentCollector, combat_diagnostics_summary
from train.model import ModelConfig, Policy
from tools.e4_continue import waiting
from tools.e4_run import atomic_checkpoint, atomic_json, collective_stop, summaries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', type=int, choices=(97, 109, 111), required=True)
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
    source_commits = {97: 'd9539cc5805efe77e76031eb1a83a1fd69b4784b',
                      109: 'f2db1e248e9c0e076646edfe130eadb5158eedd3',
                      111: '4fe2af0836959dd592d333ec267cdbdfce66d313'}
    assert source['commit'] == source_commits[args.version] and source['version'] == args.version
    assert source['initialization'] == 1729 and source['lanes'] == 256 and source['trials'] == 32
    torch.manual_seed(1729)
    policy = Policy(ModelConfig()).to(device)
    policy.load_state_dict(source['model'])
    policy.eval()
    search_rng = torch.Generator(device=device).manual_seed(2729+rank)
    search = RoomSearch(256, 32, device, search_rng, scratch_slots=256*33)
    search.evidence_directory = args.output
    search.deadline = time.monotonic()+args.seconds-120
    search.combat_diagnostics = []
    collector = SegmentCollector(search, torch.Generator(), rank=rank)
    checkpoint_path = args.output / f'rank-{rank}.pt'
    complete, elapsed = False, 0.
    if checkpoint_path.exists():
        saved = torch.load(checkpoint_path, map_location='cpu', weights_only=False)
        assert saved['commit'] == commit and saved['source_sha256'] == source_hash
        collector.restore(saved['collector'])
        search.combat_diagnostics = saved['diagnostics']
        search.metrics = saved['search_metrics']
        search.capacity_metrics = saved['capacity_metrics']
        complete, elapsed = saved['complete'], saved['evaluation_seconds']

    def checkpoint(reason):
        atomic_checkpoint(checkpoint_path, dict(commit=commit, source_sha256=source_hash,
            version=args.version, complete=complete, reason=reason, collector=collector.state(),
            diagnostics=search.combat_diagnostics, search_metrics=search.metrics,
            capacity_metrics=search.capacity_metrics, evaluation_seconds=elapsed))
        dist.barrier()
        if rank == 0:
            atomic_json(args.output/'status.json', dict(commit=commit, version=args.version,
                job=os.environ['SLURM_JOB_ID'], complete=complete, reason=reason, evaluation_seconds=elapsed))

    try:
        if complete:
            return
        checkpoint('started')
        if waiting(rank, args.output, args.version):
            checkpoint('yield_to_waiting_user')
            return
        started = time.monotonic()
        expired = False
        seeds = list(range(2000000000+rank, 2000000256, world))
        try:
            episodes = (collector.completed if collector.evaluation_waiting is None
                        and len(collector.completed) == len(seeds) else collector.evaluate(policy, seeds))
        except AllocationEnded:
            expired = True
        duration = torch.tensor(time.monotonic()-started, device=device)
        dist.all_reduce(duration, op=dist.ReduceOp.MAX)
        elapsed += float(duration)
        if collective_stop(expired, device):
            checkpoint('allocation_limit')
            return
        assert len(episodes) == 32 and {e['seed'] for e in episodes} == set(seeds)
        atomic_json(args.output/f'episodes-{rank}.json', episodes)
        atomic_json(args.output/f'diagnostics-{rank}.json', search.combat_diagnostics)
        atomic_json(args.output/f'limits-{rank}.json', dict(search.metrics, **search.capacity_metrics))
        dist.barrier()
        limit_exceeded = [None]
        if rank == 0:
            all_episodes = sum([json.loads((args.output/f'episodes-{r}.json').read_text()) for r in range(world)], [])
            rows = sum([json.loads((args.output/f'diagnostics-{r}.json').read_text()) for r in range(world)], [])
            limits = [json.loads((args.output/f'limits-{r}.json').read_text()) for r in range(world)]
            assert len(all_episodes) == 256 and {e['seed'] for e in all_episodes} == set(range(2000000000, 2000000256))
            capped, trials = sum(x['combat_played_cards_limit'] for x in limits), sum(x['combat_trials'] for x in limits)
            fraction = capped/trials if trials else 0.
            limit_exceeded[0] = fraction > .001
            report = dict(version=args.version, commit=commit, source_directory=str(args.source),
                evaluation_seconds=elapsed, evaluation=summaries(all_episodes), combat=combat_diagnostics_summary(rows),
                combat_played_cards_limit=capped, combat_trials=trials, capacity_fraction=fraction,
                combat_over_limit=sum(x['combat_over_limit'] for x in limits),
                event_over_limit=sum(x['event_over_limit'] for x in limits),
                definitions=dict(entropy='Natural-log policy entropy, raw nats; mean per combat and pooled over all trial decisions',
                    win='battle_over and hp>0 and not smoked', score_std='Population standard deviation; death score zero',
                    distinct_end_states='loop_key of all 32 ended trials including death',
                    coverage='All combat SL calls, including nested battles in unselected event trials',
                    capped='Incomplete 32-trial groups retain entropy and counts; four end-state metrics are null, never treated as death'))
            atomic_json(args.output/'summary.json', report)
            print(json.dumps(report), flush=True)
        dist.broadcast_object_list(limit_exceeded, src=0)
        complete = True
        checkpoint('capacity_limit_stop' if limit_exceeded[0] else 'complete')
    finally:
        if sys.exc_info()[0] is not None:
            atomic_checkpoint(args.output/f'fault-rank-{rank}.pt', dict(commit=commit,
                source_sha256=source_hash, collector=collector.state(), diagnostics=search.combat_diagnostics))
        dist.destroy_process_group()


if __name__ == '__main__':
    main()
