import argparse
from dataclasses import asdict
from datetime import timedelta
import gzip
import json
import math
import os
from pathlib import Path
import subprocess
import time

import torch
import torch.distributed as dist

from tools.e4_run import atomic_json
from train.checkpoint import engine_commit
from train.e4 import AllocationEnded, RoomSearch
from train.e4_teacher_distill import (DistillationCollector, StudentEvaluationCollector,
                              collect, fit, examination_gate)
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.construction import bind_parent_stop
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_stage6.phase6_storage import file_digest
from train.e4_teacher_pilot import GROUPS, evaluate_scenes, summarize
from train.model import ModelConfig


def allocation_deadline(context, job, world, commit):
    assert context['format'] == 1 and context['phase'] == 'claude96_stage8'
    assert context['runtime_commit'] == commit and world in (7, 8)
    assert context['operation'] in ('collect', 'learn_exam', 'evaluate')
    receipt, reserve, release, allocation = (context[k] for k in ('receipt', 'reservation', 'release', 'allocation'))
    assert all(str(r['job_id']) == job for r in (receipt, reserve, release, allocation))
    assert receipt['world'] == allocation['world'] == world and allocation['cpus'] == world + 1
    wall = allocation['wall_seconds']
    assert receipt['wall_seconds'] == wall and 0 < wall <= 14400
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
    budget = context['budgets']['claude96_stage8']
    assert budget['cap_node_seconds'] == 6000 and budget['training_authorized']
    charged, held = budget['charged_node_seconds'], budget['reserved_node_seconds']
    assert 0 <= charged and 0 <= held and charged + held <= 6000
    assert reserve['node_seconds'] == world * wall / 8 <= held
    available = 6000 - charged - (held - reserve['node_seconds'])
    end = min(allocation['end_at'], allocation['start_at'] + available * 8 / world)
    if context['operation'] == 'collect':
        labels = budget['label_collection']
        assert labels['cap_node_seconds'] == 3500
        spent, reserved = labels['charged_node_seconds'], labels['reserved_node_seconds']
        assert 0 <= spent and 0 <= reserved and spent + reserved <= 3500
        assert reserve['node_seconds'] <= reserved
        available = 3500 - spent - (reserved - reserve['node_seconds'])
        end = min(end, allocation['start_at'] + available * 8 / world)
    return time.monotonic() + end - time.time() - 210


def save_tensor(path, value):
    temporary = path.with_suffix('.tmp')
    torch.save(value, temporary)
    os.replace(temporary, path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--context', type=Path, required=True)
    args = parser.parse_args()
    context = json.loads(args.context.read_bytes())
    rank, world, local = (int(os.environ[k]) for k in ('RANK', 'WORLD_SIZE', 'LOCAL_RANK'))
    assert rank == local and int(os.environ['SLURM_JOB_NUM_NODES']) == 1
    commit = engine_commit()
    assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=Path(__file__).resolve().parents[1]).strip()
    deadline = allocation_deadline(context, os.environ['SLURM_JOB_ID'], world, commit)
    torch.set_num_threads(1)
    device = torch.device('cuda', local)
    torch.cuda.set_device(device)
    dist.init_process_group('nccl', timeout=timedelta(hours=2))
    if rank == 0:
        assert all(file_digest(Path(r['path'])) == r['sha256'] for r in context['inputs'])
    dist.barrier()
    bound = {r['path']: r['sha256'] for r in context['inputs']}
    assert bound[context['B2']['path']] == context['B2']['sha256']
    checkpoint = torch.load(context['B2']['path'], map_location='cpu', weights_only=False)
    assert checkpoint['evaluation_only']
    assert checkpoint['source_manifest_sha256'] == '370aa2bb3bbc90ca030ba43eddd643d8e5d929673d5287fcdaf16147dbd9d730'
    reference = CombatPolicy(ModelConfig(**checkpoint['config'])).to(device)
    reference.load_state_dict(checkpoint['model'], strict=True)
    reference.eval().requires_grad_(False)
    initial = policy_hash(reference)
    assert initial == context['B2']['policy_sha256']
    lanes = 256 if context['operation'] == 'evaluate' else 8
    scratch = lanes * 33 if context['operation'] == 'evaluate' else lanes * 128
    search = RoomSearch(lanes, 32, device, torch.Generator(device=device).manual_seed(96 + rank),
                        scratch_slots=scratch, bosses_seen=511)
    search.deadline = deadline
    bind_parent_stop(search, Path(context['stop_file']))
    output = Path(context['output']) / os.environ['SLURM_JOB_ID']
    if rank == 0:
        output.mkdir(parents=True, exist_ok=False)
    dist.barrier()
    search.evidence_directory = output
    if context['operation'] == 'collect':
        collector = DistillationCollector(search, torch.Generator().manual_seed(9600 + rank),
            torch.Generator(device=device).manual_seed(96 + rank), rank=rank, world=world,
            first_seed=context['training_seed_start'])
        target = 70000 // world + int(rank < 70000 % world)
        report = collect(collector, reference, target,
                         lambda data: save_tensor(output / f'labels-{rank}.pt', data))
        atomic_json(output / f'collection-{rank}.json', report)
    elif context['operation'] == 'learn_exam':
        student_path = output / 'student.pt'
        if rank == 0:
            data = dict(combat=[], noncombat=[], encounters={}, episodes=[])
            for record in context['label_shards']:
                assert bound[record['path']] == record['sha256']
                shard = torch.load(record['path'], map_location='cpu', weights_only=False)
                for name in ('combat', 'noncombat', 'episodes'):
                    data[name].extend(shard[name])
                assert not data['encounters'].keys() & shard['encounters'].keys()
                data['encounters'].update(shard['encounters'])
            complete = False
            try:
                _, history = fit(reference, data, torch.Generator().manual_seed(96), search.clock,
                    lambda value: save_tensor(student_path, value))
                complete = True
                atomic_json(output / 'learning.json', dict(complete=True, epochs=history))
            except AllocationEnded:
                atomic_json(output / 'learning.json', dict(complete=False, reason='allocation_or_parent_stop',
                    saved_complete_epochs=student_path.exists(), examination_not_started=True))
            del data
        else:
            complete = False
        ready = torch.tensor(int(complete), device=device)
        dist.broadcast(ready, src=0)
        if bool(ready):
            trained = torch.load(student_path, map_location='cpu', weights_only=False)
            student = CombatPolicy(trained['config']).to(device)
            student.load_state_dict(trained['best_model'], strict=True)
            assert asdict(student.config) == checkpoint['config']
            student.eval().requires_grad_(False)
            assert bound[context['exam']['path']] == context['exam']['sha256']
            with gzip.open(context['exam']['path'], 'rt') as stream:
                exam = json.load(stream)
            assert exam['evaluation_accepted'] and not exam['training_eligible'] and exam['bosses_seen'] == 511
            scenes = [r for g in GROUPS for r in exam['groups'][g]]
            assert len(scenes) == 990
            rows = []
            save = lambda: atomic_json(output / f'student-rows-{rank}.json', rows)
            phase = evaluate_scenes(search, student, scenes[rank::world], 'B2', rows, save)
            for row in rows:
                row['mode'] = 'student'
            save()
            atomic_json(output / f'examination-{rank}.json', phase)
            finished = torch.tensor(int(phase['complete']), device=device)
            dist.all_reduce(finished, op=dist.ReduceOp.MIN)
            if rank == 0 and bool(finished):
                all_rows = [r for i in range(world) for r in json.loads((output / f'student-rows-{i}.json').read_bytes())]
                baseline = []
                for record in context['B2_exam_rows']:
                    assert bound[record['path']] == record['sha256']
                    baseline.extend(json.loads(Path(record['path']).read_bytes())['B2'])
                atomic_json(output / 'student-examination.json', dict(grouped=summarize(all_rows),
                    gate=examination_gate(baseline, all_rows), student_sha256=policy_hash(student),
                    full_run_evaluation='Separate budget admission required; report actual remaining budget before1024.'))
    else:
        record = context['student']
        assert bound[record['path']] == record['sha256']
        trained = torch.load(record['path'], map_location='cpu', weights_only=False)
        assert trained['stopped_on_plateau']
        student = CombatPolicy(trained['config']).to(device)
        student.load_state_dict(trained['best_model'], strict=True)
        assert asdict(student.config) == checkpoint['config']
        student.eval().requires_grad_(False)
        original_student = policy_hash(student)
        collector = StudentEvaluationCollector(search, torch.Generator().manual_seed(96 + rank), rank=rank)
        seeds = list(range(2000000000 + rank, 2000001024, world))
        complete = False
        try:
            collector.evaluate(student.run_policy, seeds)
            complete = True
        except AllocationEnded:
            pass
        atomic_json(output / f'full-run-{rank}.json', dict(complete=complete, episodes=collector.completed,
                    potion_discards=collector.potion_discards, after_elite=collector.after_elite))
        assert policy_hash(student) == original_student
        done = torch.tensor(int(complete), device=device)
        dist.all_reduce(done, op=dist.ReduceOp.MIN)
        if rank == 0 and bool(done):
            episodes = [r for i in range(world) for r in json.loads((output / f'full-run-{i}.json').read_bytes())['episodes']]
            assert sorted(r['seed'] for r in episodes) == list(range(2000000000, 2000001024))
            baseline = []
            for record in context['B4145_episodes']:
                assert bound[record['path']] == record['sha256']
                baseline.extend(json.loads(Path(record['path']).read_bytes()))
            base = {r['seed']: r for r in baseline}
            assert len(base) == len(baseline) == 1024 and set(base) == {r['seed'] for r in episodes}
            baseline_mean = sum(r['floor'] for r in baseline) / 1024
            assert round(baseline_mean, 3) == 18.755
            differences = [r['floor'] - base[r['seed']]['floor'] for r in episodes]
            mean = sum(differences) / 1024
            se = math.sqrt(sum((d - mean) ** 2 for d in differences) / (1024 * 1023))
            atomic_json(output / 'full-run-summary.json', dict(episodes=1024, student_sha256=original_student,
                baseline='B4145', baseline_mean_floor=baseline_mean,
                student_mean_floor=sum(r['floor'] for r in episodes) / 1024,
                paired_floor_improvement=mean, paired_standard_error=se,
                per_game_potion_discards=[dict(seed=r['seed'], count=r['potion_discards']) for r in episodes],
                potions_after_elite=[v for r in episodes for v in r['potions_after_elite']]))
    assert policy_hash(reference) == initial and all(p.grad is None for p in reference.parameters())
    dist.barrier()
    dist.destroy_process_group()


if __name__ == '__main__':
    main()
