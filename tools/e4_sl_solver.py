import argparse
from datetime import timedelta
import gzip
import json
import os
from pathlib import Path
import subprocess
import time

import torch
import torch.distributed as dist

from tools.e4_run import atomic_json
from tools.e4_teacher import allocation_deadline
from train.checkpoint import engine_commit
from train.e4 import RoomSearch
from train.model import ModelConfig
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.construction import bind_parent_stop
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_stage6.phase6_storage import file_digest
from train.e4_sl_evaluation import evaluate_scenes, report
from train.e4_teacher_pilot import GROUPS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--context', type=Path, required=True)
    args = parser.parse_args()
    context = json.loads(args.context.read_bytes())
    assert context['phase'] == 'claude97_stage9'
    rank, world, local_rank = (int(os.environ[k]) for k in ('RANK', 'WORLD_SIZE', 'LOCAL_RANK'))
    assert rank == local_rank and int(os.environ['SLURM_JOB_NUM_NODES']) == 1
    commit = engine_commit()
    repository = Path(__file__).resolve().parents[1]
    assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=repository).strip()
    deadline = allocation_deadline(context, os.environ['SLURM_JOB_ID'], world, commit)
    torch.set_num_threads(1)
    device = torch.device('cuda', local_rank)
    torch.cuda.set_device(device)
    dist.init_process_group('nccl', timeout=timedelta(minutes=70))
    if rank == 0:
        assert all(file_digest(Path(r['path'])) == r['sha256'] for r in context['inputs'])
    dist.barrier()
    inputs = {r['path']: r['sha256'] for r in context['inputs']}
    for name in ('B2', 'exam', 'B4145_episodes'):
        assert inputs[context[name]['path']] == context[name]['sha256']
    checkpoint = torch.load(context['B2']['path'], map_location='cpu', weights_only=False)
    assert checkpoint['evaluation_only']
    policy = CombatPolicy(ModelConfig(**checkpoint['config'])).to(device)
    policy.load_state_dict(checkpoint['model'], strict=True)
    del checkpoint
    policy.eval().requires_grad_(False)
    original_policy = policy_hash(policy)
    search = RoomSearch(32, 32, device, torch.Generator(device=device).manual_seed(97),
                        scratch_slots=32 * 34, bosses_seen=511)
    search.deadline = deadline
    bind_parent_stop(search, Path(context['stop_file']))
    output = Path(context['output']) / os.environ['SLURM_JOB_ID']
    if rank == 0:
        output.mkdir(parents=True, exist_ok=False)
    dist.barrier()
    with gzip.open(context['exam']['path'], 'rt') as stream:
        exam = json.load(stream)
    assert exam['evaluation_accepted'] and not exam['training_eligible'] and exam['bosses_seen'] == 511
    assert sum(len(exam['groups'][group]) for group in GROUPS) == 990
    with gzip.open(context['B4145_episodes']['path'], 'rt') as stream:
        baseline_episodes = json.load(stream)
    rows = {mode: [] for mode in ('B2', 'N32', 'S')}
    batches = []
    def save():
        atomic_json(output / f'rank-{rank}-rows.json', rows)
        atomic_json(output / f'rank-{rank}-batches.json', batches)
    save()
    first = [s for group in GROUPS for s in exam['groups'][group][:50]]
    started = time.monotonic()
    complete = evaluate_scenes(search, policy, first[rank::world], rows, batches, save)
    elapsed = torch.tensor(time.monotonic() - started, device=device)
    dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
    extend = torch.tensor(int(complete and time.monotonic() + float(elapsed) * 740 / 250 < deadline - 60), device=device)
    dist.all_reduce(extend, op=dist.ReduceOp.MIN)
    if bool(extend):
        remaining = [s for group in GROUPS for s in exam['groups'][group][50:]]
        evaluate_scenes(search, policy, remaining[rank::world], rows, batches, save)
    assert policy_hash(policy) == original_policy and all(p.grad is None for p in policy.parameters())
    atomic_json(output / f'rank-{rank}-report.json', dict(report(rows, batches, exam, baseline_episodes),
        rank=rank, world=world, runtime_commit=commit, policy_sha256=original_policy))
    dist.barrier()
    if rank == 0:
        all_rows = {mode: [] for mode in rows}
        all_batches = []
        for worker in range(world):
            data = json.loads((output / f'rank-{worker}-rows.json').read_bytes())
            for mode in rows:
                all_rows[mode].extend(data[mode])
            all_batches.extend(json.loads((output / f'rank-{worker}-batches.json').read_bytes()))
        atomic_json(output / 'summary.json', dict(report(all_rows, all_batches, exam, baseline_episodes),
            runtime_commit=commit, policy_sha256=original_policy,
            coverage={mode: len(data) for mode, data in all_rows.items()},
            full_hard990_attempted=bool(extend)))
    dist.barrier()
    dist.destroy_process_group()


if __name__ == '__main__':
    main()
