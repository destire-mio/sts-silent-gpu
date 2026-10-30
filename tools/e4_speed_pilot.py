"""One fresh B2 timing update and the paired Stage10 dev256 evaluation."""
import argparse
from collections import Counter
from copy import deepcopy
from datetime import timedelta
import gc
import gzip
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from tools.e4_run import atomic_json
from tools.e4_teacher import allocation_deadline
from train.checkpoint import engine_commit
from train.e4 import AllocationEnded, MAX_REWARD, RoomSearch, SegmentCollector
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.construction import bind_parent_stop
from train.e4_stage6.copy_search import CopyCollector, CopySearch
from train.e4_stage6.evaluation_worker import policy_hash
from train.e4_stage6.phase6_storage import file_digest
from train.e4_stage6.training_worker import TrainingWorker, move_tensors
from train.e4_timing import timing
from train.e4_timing import forward_autocast
from train.model import ModelConfig, distribution, packed_inputs, record_lengths
from train.e4_stage6.copy_learner import MICROBATCH, MINIBATCH


def pilot_seeds():
    return list(range(2000000000, 2000000254)) + [2000000879, 2000000402]


def paired_summary(episodes, baseline, *, complete):
    current = {e['seed']: e for e in episodes}
    previous = {e['seed']: e for e in baseline}
    assert len(current) == len(episodes) and len(previous) == len(baseline)
    expected = set(pilot_seeds())
    assert set(current) <= expected <= set(previous)
    assert not complete or set(current) == expected
    deltas = [e['floor'] - previous[seed]['floor'] for seed, e in current.items()]
    se = statistics.stdev(deltas) / math.sqrt(len(deltas)) if len(deltas) > 1 else None
    delta = statistics.mean(deltas) if deltas else None
    return dict(complete=complete, cases=len(deltas), expected_cases=256,
        mean_floor=statistics.mean(e['floor'] for e in episodes) if episodes else None,
        paired_B2_mean_floor=statistics.mean(previous[s]['floor'] for s in current) if current else None,
        paired_floor_delta=delta, paired_floor_SE=se,
        no_regression_beyond_one_paired_SE=(delta >= -se) if complete and se is not None else None,
        stalled=sum(e['outcome'] == 'stalled' for e in episodes),
        potion_discard_frames_with_repeat_blocks=sum(e['noncombat']['counts'].get(
            'out_of_combat_potion_discards_with_repeat_blocks', 0) for e in episodes),
        shop_cases={str(s): dict(floor=current[s]['floor'], outcome=current[s]['outcome'],
            noncombat=current[s]['noncombat']) for s in (2000000879, 2000000402) if s in current},
        comparison='Paired existing frozen B2 full-run episodes; changed search RNG ordering is allowed.')


def b2_worker(source, expected_policy_hash, device, runtime_commit, evidence, stop_file):
    """Retain B2 model/Adam/beta; discard old biased ready data and collector/RNG."""
    rank, world = dist.get_rank(), dist.get_world_size()
    assert source['format'] == 1 and source['binding']['arm'] == 'B'
    assert source['state']['updates'] == 7 and source['cursor'] is None
    policy = CombatPolicy(ModelConfig(**source['binding']['config'])).to(device)
    policy.load_state_dict(source['model'], strict=True)
    assert policy_hash(policy) == expected_policy_hash
    optimizer = torch.optim.Adam(policy.parameters(), lr=1e-4)
    optimizer.load_state_dict(deepcopy(source['optimizer']))
    assert all(g['lr'] == 1e-4 for g in optimizer.param_groups)
    steps = Counter(int(s['step']) for s in optimizer.state.values())
    assert len(optimizer.state) == sum(len(g['params']) for g in optimizer.param_groups)
    assert steps and min(steps) > 0
    model = DistributedDataParallel(policy, device_ids=[device.index] if device.type == 'cuda' else None,
                                   broadcast_buffers=False)
    # New independent streams are explicit; no old collector or cached fragment is restored.
    seed = 20261030 + rank
    torch.manual_seed(seed)
    search = CopySearch(256, device, torch.Generator(device=device).manual_seed(seed + 100),
                        torch.Generator().manual_seed(seed + 200), bosses_seen=511)
    search.evidence_directory = evidence
    bind_parent_stop(search, stop_file)
    collector = CopyCollector(search, torch.Generator().manual_seed(seed + 300), rank=rank)
    binding = dict(arm='B', rank=rank, world=world, bosses_seen=511, lanes=256, trials=32,
        epochs=4, runtime_commit=runtime_commit, config=source['binding']['config'],
        source_world=source['binding']['world'], source_rank=0, source_update=7,
        source_Adam_step_counts=dict(sorted(steps.items())), source_policy_sha256=expected_policy_hash,
        initialization_seed=seed, initialization='B2 model/Adam/beta; fresh collector and RNG; old ready discarded',
        timing_only=True, allowed_updates=1)
    return TrainingWorker(model, optimizer, collector,
        torch.Generator(device=device).manual_seed(seed + 400), binding,
        source['state']['beta'], source['state']['entropy_target'])


@torch.no_grad()
def first_minibatch_ratios(worker):
    cursor = worker.cursor
    assert cursor['epoch'] == cursor['start'] == cursor['optimizer_steps'] == 0
    rows, device = cursor['rows'], worker.device
    count = MINIBATCH // dist.get_world_size()
    selected, valid = cursor['order'][:count], cursor['valid'][:count]
    entities, candidates = record_lengths(rows['data'], rows['offsets'], selected)
    order = torch.argsort((entities // 16) * 129 + candidates, stable=True)
    selected, valid = selected[order], valid[order]
    def measure():
        total = torch.zeros(6, dtype=torch.float64, device=device)
        for start in range(0, len(selected), MICROBATCH):
            indices = selected[start:start + MICROBATCH]
            inputs = packed_inputs(rows['data'], rows['offsets'], indices, device)
            inputs['candidate_mask'] &= rows['policy_mask'][indices, :inputs['candidate_mask'].shape[1]]
            with forward_autocast(device):
                logits, _, _ = worker.model.module(inputs)
            policy, _ = distribution(logits.float(), inputs['candidate_mask'])
            log_ratio = policy.log_prob(rows['actions'][indices]) - rows['logs'][indices]
            assert bool(torch.isfinite(log_ratio).all())
            present = valid[start:start + MICROBATCH]
            ppo = present & rows['ppo'][indices]
            clipped = (log_ratio.exp() - 1).abs() > .2
            total += torch.stack(((log_ratio.abs() * present).double().sum(),
                (clipped & present).double().sum(), present.double().sum(),
                (log_ratio.abs() * ppo).double().sum(), (clipped & ppo).double().sum(), ppo.double().sum()))
        dist.all_reduce(total)
        values = total.tolist()
        assert values[2] > 0
        return dict(mean_abs_log_ratio=values[0] / values[2], clip_fraction=values[1] / values[2],
            rows=int(values[2]), PPO_mean_abs_log_ratio=values[3] / values[5] if values[5] else None,
            PPO_clip_fraction=values[4] / values[5] if values[5] else None, PPO_rows=int(values[5]),
            scope='Global first minibatch before any optimizer step; valid rows, with PPO subset separately')
    result = dict(initial=measure(), encoder_bf16_heads_fp32_correction=False)
    before = result['initial']
    if max(before['mean_abs_log_ratio'], before['PPO_mean_abs_log_ratio'] or 0.) > .005 or \
            max(before['clip_fraction'], before['PPO_clip_fraction'] or 0.) > .01:
        worker.model.module.forward = worker.model.module.encoder_bf16_forward
        result['encoder_bf16_heads_fp32_correction'] = True
        result['corrected'] = measure()
    worker.binding['first_minibatch_ratios'] = result
    return result


def one_update(worker, deadline):
    assert worker.state['updates'] == 0 and worker.ready is None and worker.cursor is None
    timing.seconds = dict.fromkeys(timing.seconds, 0.)
    timing.counts = dict.fromkeys(timing.counts, 0)
    timing.enabled = True
    worker.synchronize()
    started = time.perf_counter()
    try:
        while worker.state['updates'] == 0:
            result = worker.step(deadline)
            if result.get('action') == 'fragment_ready':
                if worker.device.type == 'cuda' and torch.cuda.get_device_capability(worker.device)[0] >= 8:
                    first_minibatch_ratios(worker)
                else:
                    worker.binding['first_minibatch_ratios'] = dict(skipped=True,
                        reason='BF16 disabled on this hardware; Claude104 does not require the ratio probe')
            if not result['performed_training']:
                break
    finally:
        worker.synchronize()
        elapsed = time.perf_counter() - started
        timing.enabled = False
    assert worker.state['updates'] in (0, 1)
    report = timing.report(elapsed, worker.state['updates'] == 1)
    report.update(baseline_job=4153, baseline_update_seconds=245.,
        baseline_labels_seconds=dict(combat_sl=131., learning=76., repeat_check=42., event_sl=17.),
        under_100_seconds=elapsed < 100 if worker.state['updates'] == 1 else None)
    return report


def restore_B2_for_evaluation(policy, checkpoint_path, expected_hash):
    frozen = torch.load(checkpoint_path, map_location='cpu', weights_only=False)
    assert frozen['evaluation_only']
    policy.load_state_dict(frozen['model'], strict=True)
    policy.forward = type(policy).forward.__get__(policy)
    policy.eval().requires_grad_(False)
    policy.zero_grad(set_to_none=True)
    assert policy_hash(policy) == expected_hash
    return expected_hash


class CalibratedCollector(SegmentCollector):
    """Use values already produced by choose; no extra forward or rollout."""
    def __init__(self, *args, save, **kwargs):
        super().__init__(*args, **kwargs)
        self.predictions = {}
        self.calibration = []
        self.save = save

    def choose(self, model, slots, progress):
        result = super().choose(model, slots, progress)
        for slot, value in zip(slots, result[1]):
            self.predictions.setdefault(self.seeds[slot], []).append(
                (value, self.rewards[slot].total / MAX_REWARD))
        return result

    def advance(self, slots, actions, progress):
        before = len(self.completed)
        result = super().advance(slots, actions, progress)
        for episode in self.completed[before:]:
            samples = self.predictions.pop(episode['seed'])
            predicted = [p for p, _ in samples]
            target = [episode['reward'] - accrued for _, accrued in samples]
            self.calibration.append(dict(seed=episode['seed'], decisions=len(samples),
                predicted_sum=sum(predicted), realized_remaining_sum=sum(target),
                squared_error_sum=sum((p - t) ** 2 for p, t in zip(predicted, target)),
                scope='Original B2 mainline value vs realized remaining shaped reward; completed games only'))
        if len(self.completed) // 8 != before // 8:
            self.save(self)
        return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--context', type=Path, required=True)
    args = parser.parse_args()
    context = json.loads(args.context.read_bytes())
    assert context['phase'] == 'claude99_stage10'
    rank, world, local_rank = (int(os.environ[k]) for k in ('RANK', 'WORLD_SIZE', 'LOCAL_RANK'))
    assert rank == local_rank and int(os.environ['SLURM_JOB_NUM_NODES']) == 1
    repository = Path(__file__).resolve().parents[1]
    commit = engine_commit()
    assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=repository).strip()
    deadline = allocation_deadline(context, os.environ['SLURM_JOB_ID'], world, commit)
    torch.set_num_threads(1)
    device = torch.device('cuda', local_rank)
    torch.cuda.set_device(device)
    dist.init_process_group('nccl', timeout=timedelta(minutes=25))
    if rank == 0:
        assert all(file_digest(Path(r['path'])) == r['sha256'] for r in context['inputs'])
    dist.barrier()
    inputs = {r['path']: r['sha256'] for r in context['inputs']}
    for name in ('B2', 'B2_hot', 'B2_episodes'):
        assert inputs[context[name]['path']] == context[name]['sha256']
    frozen = torch.load(context['B2']['path'], map_location='cpu', weights_only=False)
    assert frozen['evaluation_only'] and frozen['source_manifest_sha256'] == \
        '370aa2bb3bbc90ca030ba43eddd643d8e5d929673d5287fcdaf16147dbd9d730'
    with gzip.open(context['B2_episodes']['path'], 'rt') as stream:
        baseline = json.load(stream)
    paired_summary([], baseline, complete=False)
    source = torch.load(context['B2_hot']['path'], map_location='cpu', weights_only=False)
    assert source['binding']['config'] == frozen['config']
    output = Path(context['output']) / os.environ['SLURM_JOB_ID']
    if rank == 0:
        output.mkdir(parents=True, exist_ok=False)
    dist.barrier()
    worker = b2_worker(source, context['B2']['policy_sha256'], device, commit, output, Path(context['stop_file']))
    del frozen, source
    measured = one_update(worker, deadline)
    policy = worker.model.module
    after_hash = policy_hash(policy)
    atomic_json(output / f'rank-{rank}-timing.json', dict(rank=rank, world=world,
        binding=worker.binding, state=worker.state, policy_sha256=after_hash, **measured))
    if rank == 0:
        torch.save(move_tensors(dict(format=1, timing_only=True, binding=worker.binding,
            state=worker.state, model=policy.state_dict(), optimizer=worker.optimizer.state_dict()),
            torch.device('cpu')), output / 'timing-update.pt')
    complete_update = measured['complete_update']
    del worker
    gc.collect()
    torch.cuda.empty_cache()
    evaluation_hash = restore_B2_for_evaluation(policy, context['B2']['path'], context['B2']['policy_sha256'])
    search = RoomSearch(256, 32, device, torch.Generator(device=device).manual_seed(101 + rank),
                        scratch_slots=256 * 33, inference_chunk=1024, bosses_seen=511)
    search.deadline = deadline
    search.evidence_directory = output
    bind_parent_stop(search, Path(context['stop_file']))
    def save(collector):
        atomic_json(output / f'rank-{rank}-episodes.json', collector.completed)
        atomic_json(output / f'rank-{rank}-calibration.json', collector.calibration)
    collector = CalibratedCollector(search, torch.Generator().manual_seed(101 + rank), rank=rank, save=save)
    complete_eval = False
    if complete_update:
        try:
            collector.evaluate(policy.run_policy, pilot_seeds()[rank::world])
            assert sorted(e['seed'] for e in collector.completed) == sorted(pilot_seeds()[rank::world])
            complete_eval = True
        except AllocationEnded:
            pass
    save(collector)
    assert evaluation_hash == policy_hash(policy) and all(p.grad is None for p in policy.parameters())
    atomic_json(output / f'rank-{rank}-evaluation.json', dict(complete=complete_eval,
        policy_sha256=evaluation_hash, parameter_updates=0, timing_parameter_updates=int(complete_update),
        loop_metrics=search.loop_metrics, capacity_metrics=search.capacity_metrics))
    dist.barrier()
    if rank == 0:
        rows, calibration, timers, reports = [], [], [], []
        for r in range(world):
            rows.extend(json.loads((output / f'rank-{r}-episodes.json').read_bytes()))
            calibration.extend(json.loads((output / f'rank-{r}-calibration.json').read_bytes()))
            timers.append(json.loads((output / f'rank-{r}-timing.json').read_bytes()))
            reports.append(json.loads((output / f'rank-{r}-evaluation.json').read_bytes()))
        assert {r['policy_sha256'] for r in reports} == {context['B2']['policy_sha256']}
        wall = max(r['measured_wall_seconds'] for r in timers)
        decisions = sum(r['decisions'] for r in calibration)
        atomic_json(output / 'summary.json', dict(runtime_commit=commit, timing_only=True,
            parameter_updates=int(complete_update), production_updates=0,
            evaluation_policy_sha256=evaluation_hash, evaluation_uses_original_B2=True,
            learner_forward_dtype='bfloat16' if torch.cuda.get_device_capability(device)[0] >= 8 else 'float32',
            timing=dict(per_rank=timers, slowest_rank_seconds=wall,
                        baseline_job=4153, baseline_seconds=245., under_100_seconds=wall < 100),
            evaluation=paired_summary(rows, baseline, complete=all(r['complete'] for r in reports)),
            value_calibration=dict(decisions=decisions,
                mean_prediction=sum(r['predicted_sum'] for r in calibration) / decisions if decisions else None,
                mean_realized_remaining=sum(r['realized_remaining_sum'] for r in calibration) / decisions if decisions else None,
                MSE=sum(r['squared_error_sum'] for r in calibration) / decisions if decisions else None,
                scope='Original B2 evaluation decisions, without additional GPU runs/forwards'),
            allocation_node_seconds=None, settlement='Parent actual final Slurm GPU-seconds/8 pending'))
    dist.barrier()
    dist.destroy_process_group()


if __name__ == '__main__':
    main()
