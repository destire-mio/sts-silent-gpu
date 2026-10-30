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
from train.checkpoint import engine_commit
from train.e4 import RoomSearch
from train.model import ModelConfig
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.construction import bind_parent_stop
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_stage6.phase6_storage import file_digest
from train.e4_teacher_pilot import GROUPS, evaluate_scenes, summarize, paired_rows


def allocation_deadline(context, job, world, commit):
    assert context['format'] == 1
    cap = {'claude94_stage7': 3600, 'claude97_stage9': 3600, 'claude99_stage10': 1200}[context['phase']]
    assert context['runtime_commit'] == commit and world in (7, 8)
    receipt, reserve, release, allocation = (context[k] for k in ('receipt', 'reservation', 'release', 'allocation'))
    assert all(str(r['job_id']) == job for r in (receipt, reserve, release, allocation))
    assert receipt['world'] == allocation['world'] == world and allocation['cpus'] == world + 1
    wall = allocation['wall_seconds']
    assert 0 < wall <= 14400 and receipt['wall_seconds'] == wall
    assert allocation['end_at'] - allocation['start_at'] == wall
    assert 0 <= receipt['at'] <= reserve['at'] <= release['at'] <= allocation['start_at'] <= time.time()
    for boundary in (receipt['at'], release['at']):
        assert 0 <= boundary - release['budget_at'] <= 300
        assert 0 <= boundary - release['preflight_at'] <= 300
    path = Path(receipt['stdout']['path'])
    assert file_digest(path) == receipt['stdout']['sha256']
    assert path.read_text().strip() == 'Submitted batch job ' + job
    for record in (reserve['parent_ledger'], release['preflight'], allocation['scontrol']):
        assert file_digest(Path(record['path'])) == record['sha256']
    budget = context['budgets'][context['phase']]
    assert budget['cap_node_seconds'] == cap
    charged, held = budget['charged_node_seconds'], budget['reserved_node_seconds']
    assert 0 <= charged and 0 <= held and charged + held <= cap
    assert reserve['node_seconds'] == world * wall / 8 <= held
    available = cap - charged - (held - reserve['node_seconds'])
    end = min(allocation['end_at'], allocation['start_at'] + available * 8 / world)
    return time.monotonic() + end - time.time() - 210


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--context', type=Path, required=True)
    args = parser.parse_args()
    context = json.loads(args.context.read_bytes())
    rank, world = int(os.environ['RANK']), int(os.environ['WORLD_SIZE'])
    local_rank = int(os.environ['LOCAL_RANK'])
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
    assert inputs[context['B2']['path']] == context['B2']['sha256']
    assert inputs[context['exam']['path']] == context['exam']['sha256']
    checkpoint = torch.load(context['B2']['path'], map_location='cpu', weights_only=False)
    assert checkpoint['evaluation_only'] and checkpoint['source_manifest_sha256'] == '370aa2bb3bbc90ca030ba43eddd643d8e5d929673d5287fcdaf16147dbd9d730'
    policy = CombatPolicy(ModelConfig(**checkpoint['config'])).to(device)
    policy.load_state_dict(checkpoint['model'], strict=True)
    del checkpoint
    policy.eval().requires_grad_(False)
    original_policy = policy_hash(policy)
    search = RoomSearch(8, 32, device, torch.Generator(device=device).manual_seed(94),
                        scratch_slots=8 * 128, bosses_seen=511)
    search.deadline = deadline
    bind_parent_stop(search, Path(context['stop_file']))
    output = Path(context['output']) / os.environ['SLURM_JOB_ID']
    if rank == 0:
        output.mkdir(parents=True, exist_ok=False)
    dist.barrier()
    search.evidence_directory = output
    with gzip.open(context['exam']['path'], 'rt') as stream:
        exam = json.load(stream)
    assert exam['evaluation_accepted'] and not exam['training_eligible'] and exam['bosses_seen'] == 511
    scenes = [scene for group in GROUPS for scene in exam['groups'][group]]
    assert len(scenes) == 990
    own = scenes[rank::world]
    rows = {mode: [] for mode in ('B2', 'T', 'T2')}
    def save():
        atomic_json(output / f'rank-{rank}-rows.json', rows)
    save()
    reports = {}
    for mode in ('B2', 'T'):
        reports[mode] = evaluate_scenes(search, policy, own, mode, rows[mode], save)
    first = [scene for group in GROUPS for scene in exam['groups'][group][:50]]
    reports['T2_first50'] = evaluate_scenes(search, policy, first[rank::world], 'T2', rows['T2'], save)
    elapsed = torch.tensor(reports['T2_first50']['elapsed_seconds'], device=device)
    dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
    can_extend = torch.tensor(int(reports['T2_first50']['complete'] and
        time.monotonic() + float(elapsed) * (990 - 250) / 250 < deadline - 60), device=device)
    dist.all_reduce(can_extend, op=dist.ReduceOp.MIN)
    if bool(can_extend):
        remaining = [scene for group in GROUPS for scene in exam['groups'][group][50:]]
        reports['T2_remaining'] = evaluate_scenes(search, policy, remaining[rank::world], 'T2', rows['T2'], save)
    assert policy_hash(policy) == original_policy and all(p.grad is None for p in policy.parameters())
    report = dict(rank=rank, world=world, runtime_commit=commit, policy_sha256=original_policy,
        evaluation_only=True, parameter_updates=0, source_manifest_sha256=context['B2']['source_manifest_sha256'],
        phases=reports, grouped={mode: summarize(data) for mode, data in rows.items()},
        T2_scope='full_hard990_attempted' if bool(can_extend) else 'first50_each_group_under_time_budget',
        node_seconds_accounting='Parent final detailed Slurm allocated GPU seconds/8, including all overhead and interruptions; no per-rank counter settlement.')
    atomic_json(output / f'rank-{rank}-report.json', report)
    dist.barrier()
    if rank == 0:
        all_rows = {mode: [] for mode in rows}
        for worker in range(world):
            data = json.loads((output / f'rank-{worker}-rows.json').read_bytes())
            for mode in rows:
                all_rows[mode].extend(data[mode])
        atomic_json(output / 'summary.json', dict(evaluation_only=True, parameter_updates=0,
            coverage={mode:len(data) for mode,data in all_rows.items()},
            grouped={mode:summarize(data) for mode,data in all_rows.items()},
            paired={mode:paired_rows(all_rows['B2'],all_rows[mode]) for mode in ('T','T2')},
            actual_allocation_node_seconds=None, settlement_status='parent_final_Slurm_pending',
            judgment='Claude94 decides the next step; no automatic training authorization.'))
    dist.barrier()
    dist.destroy_process_group()


if __name__ == '__main__':
    main()
