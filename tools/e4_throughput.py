import argparse
from collections import deque
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
from train.e4 import AllocationEnded, RoomSearch, SegmentCollector
from train.e4_learner import update
from train.model import ModelConfig, Policy
from tools.e4_run import atomic_checkpoint, atomic_json, collective_stop, queue_waiting

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--lanes', type=int, required=True)
p.add_argument('--trials', type=int, required=True)
p.add_argument('--seconds', type=int, required=True)
a = p.parse_args()
rank = int(os.environ['RANK'])
assert int(os.environ['WORLD_SIZE']) == 8
assert (a.lanes, a.trials) in ((256, 32), (512, 32), (512, 8))
torch.set_num_threads(1)
device = torch.device('cuda', int(os.environ['LOCAL_RANK']))
torch.cuda.set_device(device)
dist.init_process_group('nccl', timeout=timedelta(minutes=60))
a.output.mkdir(parents=True, exist_ok=True)
source = a.source / f'rank-{rank}.pt'
saved = torch.load(source, map_location='cpu', weights_only=False)
assert saved['commit'] == engine_commit() == '7c918ddab4039750b3104b96f131155f17ddfce0'
assert saved['initialization'] == 1729 and saved['version'] == 88 and saved['lanes'] == 32
assert saved['phase'] == 'training' and saved['ready'] is None
assert saved['collector']['partial'] is None and saved['collector']['evaluation_waiting'] is None
policy = Policy(ModelConfig()).to(device)
ddp = DistributedDataParallel(policy, device_ids=[device.index], broadcast_buffers=False)
policy.load_state_dict(saved['model'])
optimizer = torch.optim.Adam(policy.parameters(), lr=3e-4)
optimizer.load_state_dict(saved['optimizer'])
ppo_rng = torch.Generator(device=device)
ppo_rng.set_state(saved['ppo_rng'])
search_rng = torch.Generator(device=device)
seed_rng = torch.Generator()
search = RoomSearch(a.lanes, a.trials, device, search_rng,
                    scratch_slots=max(4096, a.lanes * 33))
search.evidence_directory = a.output
collector = SegmentCollector(search, seed_rng, rank=rank)
state = deepcopy(saved['collector'])
empty = collector.state()
for key in ('seeds', 'paths', 'floor_steps', 'noncombat_floor_steps', 'tails',
            'map_returned', 'pending', 'rewards'):
    assert len(state[key]) == 32
    state[key].extend(deepcopy(empty[key][32:]))
collector.restore(state)
torch.set_rng_state(saved['torch_rng'])
torch.cuda.set_rng_state(saved['cuda_rng'], device)
health_history = deque(saved['health_history'], maxlen=20)
started = time.monotonic()
search.deadline = started + a.seconds - 120
free, total = torch.cuda.mem_get_info(device)
non_torch = total - free - torch.cuda.memory_reserved(device)
results = []
ready = None

def checkpoint(reason):
    atomic_checkpoint(a.output / f'rank-{rank}.pt', dict(
        reason=reason, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        commit=engine_commit(), initialization=1729, source_version=88,
        benchmark_updates=len(results), lanes=a.lanes, trials=a.trials,
        model=policy.state_dict(), optimizer=optimizer.state_dict(),
        collector=collector.state(), ppo_rng=ppo_rng.get_state(),
        torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device),
        ready=ready, health_history=list(health_history)))
    dist.barrier()
    if rank == 0:
        atomic_json(a.output / 'status.json', dict(reason=reason, updates=len(results),
            lanes=a.lanes, trials=a.trials, job=os.environ['SLURM_JOB_ID'], results=results,
            source_directory=str(a.source), source_version=88,
            collector_resize='Preserve all32 source lanes, paths, pending selected actions, rewards and RNG; append empty lanes drawing next train seeds',
            scope='Diagnostic branches only; original production checkpoint is unchanged'))

try:
    checkpoint('started')
    for index in range(10):
        waiting = [queue_waiting() if rank == 0 else None]
        dist.broadcast_object_list(waiting, src=0)
        if waiting[0]:
            checkpoint('yield_to_waiting_user')
            break
        if collective_stop(time.monotonic() >= search.deadline, device):
            checkpoint('allocation_limit')
            break
        torch.cuda.reset_peak_memory_stats(device)
        before = collector.decisions
        sl_before = search.searched_decisions
        start = time.monotonic()
        expired = False
        policy.eval()
        try:
            ready = collector.collect(policy)
        except AllocationEnded:
            expired = True
        if collective_stop(expired, device):
            checkpoint('allocation_limit')
            break
        ddp.train()
        metrics = update(ddp, optimizer, ready, ppo_rng, epochs=1)
        health = ready['health'].clone()
        dist.all_reduce(health[:3], op=dist.ReduceOp.SUM)
        dist.all_reduce(health[3:], op=dist.ReduceOp.MAX)
        completed, floor_sum, stalled, max_steps = health.cpu().tolist()
        health_history.append((completed, stalled, {}))
        elapsed = torch.tensor(time.monotonic() - start, device=device)
        dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
        counts = torch.tensor([collector.decisions-before, search.searched_decisions-sl_before], device=device)
        dist.all_reduce(counts, op=dist.ReduceOp.SUM)
        peak = torch.tensor([torch.cuda.max_memory_allocated(device), torch.cuda.max_memory_reserved(device), non_torch + torch.cuda.max_memory_reserved(device)], device=device)
        dist.all_reduce(peak, op=dist.ReduceOp.MAX)
        row = dict(event='benchmark_update', update=index+1, lanes=a.lanes, trials=a.trials,
            update_seconds=float(elapsed), actual_steps=int(counts[0]), sl_trial_steps=int(counts[1]),
            actual_steps_per_second=float(counts[0]/elapsed), sl_trial_steps_per_second=float(counts[1]/elapsed),
            peak_torch_allocated_bytes=int(peak[0]), peak_torch_reserved_bytes=int(peak[1]), estimated_peak_device_bytes=int(peak[2]),
            completed_episodes=completed, stalled_episodes=stalled, active_max_floor_steps=max_steps, **metrics)
        results.append(row)
        ready = None
        if rank == 0:
            print(json.dumps(row), flush=True)
        count = sum(x[0] for x in health_history)
        stops = sum(x[1] for x in health_history)
        if count == 0 or stops * 20 > count:
            checkpoint('training_health_stop')
            break
        checkpoint('update_complete')
    else:
        checkpoint('ten_updates_complete')
finally:
    if sys.exc_info()[0] is not None:
        atomic_checkpoint(a.output / f'fault-rank-{rank}.pt', dict(
            collector=collector.state(), model=policy.state_dict(), optimizer=optimizer.state_dict(),
            ppo_rng=ppo_rng.get_state(), lanes=a.lanes, trials=a.trials))
    dist.destroy_process_group()
