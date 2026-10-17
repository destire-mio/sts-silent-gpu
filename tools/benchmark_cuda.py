"""One-GPU stepping and encoding throughput, without Game transfers or inference."""
import argparse
import json
import time
from pathlib import Path

import torch
import spire_gpu
from benchmark_training import deep_routes


def measure(size, seconds, deep):
    roots = deep[0] if deep else list(range(1000000000, 1000000000 + size))
    seeds = roots * (size * 64 // len(roots) + 1)
    batch = spire_gpu.TrainingBatch(size, 3000, seeds)
    counts = torch.as_tensor(batch.candidate_counts, device='cuda')
    indices = torch.as_tensor(batch.seed_indices, device='cuda')
    decisions = torch.as_tensor(batch.decision_counts, device='cuda')
    advanced = torch.as_tensor(batch.advanced, device='cuda')
    faults = torch.as_tensor(batch.faults, device='cuda')
    done = torch.as_tensor(batch.done, device='cuda')
    caps = torch.as_tensor(batch.truncated, device='cuda')
    routes = torch.as_tensor(deep[1], device='cuda', dtype=torch.int32) if deep else None
    total = torch.zeros(3, dtype=torch.int64, device='cuda')
    faulted = torch.zeros((), dtype=torch.bool, device='cuda')
    torch.manual_seed(1729)

    def step():
        if deep:
            selected = routes[indices % len(roots), decisions]
        else:
            selected = (torch.rand(size, device='cuda') * counts.clamp_min(1)).to(torch.int32)
        batch.step(selected)

    for _ in range(10):
        step()
    torch.cuda.synchronize()
    start = time.perf_counter()
    iterations = 0
    minimum = deep[1].shape[1] - 10 if deep else 400
    while iterations < minimum or time.perf_counter() - start < seconds:
        step()
        total[0] += advanced.sum(); total[1] += done.sum(); total[2] += caps.sum()
        faulted |= (faults != 0).any()
        iterations += 1
        if iterations % 256 == 0:
            assert not faulted.item(), 'CUDA environment fault (including seed exhaustion)'
            batch.extend_seeds(seeds)
    torch.cuda.synchronize()
    elapsed = time.perf_counter() - start
    assert not faulted.item(), 'CUDA environment fault'
    steps, terminals, truncated = total.cpu().tolist()
    assert truncated == 0
    print(json.dumps(dict(mode='deep' if deep else 'random', batch_size=size, seconds=elapsed,
                          decisions=steps, decisions_per_second=steps / elapsed, batches=iterations,
                          terminal_episodes=terminals, truncated=truncated, faults=0,
                          includes='GPU selection + dispatch + reset + step + encode + synchronization',
                          excludes='network inference, rollout storage, learning', game_downloads=0)), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sizes', nargs='+', type=int, default=[32, 128, 512, 2048])
    parser.add_argument('--seconds', type=float, default=5)
    args = parser.parse_args()
    assert torch.cuda.device_count() == 1, 'Expose exactly one GPU with CUDA_VISIBLE_DEVICES'
    print(json.dumps(dict(gpu=torch.cuda.get_device_name(), torch=torch.__version__, cuda=torch.version.cuda)), flush=True)
    deep = deep_routes(Path(__file__).resolve().parents[1] / 'tests/deep_benchmark.actions')
    for workload in (None, deep):
        for size in args.sizes:
            measure(size, args.seconds, workload)
