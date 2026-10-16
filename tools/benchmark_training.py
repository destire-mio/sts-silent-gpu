"""CPU batch benchmark: uniform candidates or accepted full-run action routes."""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import time

import numpy as np
import spire


def deep_routes(path):
    words = iter(map(int, path.read_text().split()))
    routes, seeds, acts = [], [], []
    names = ("play", "end_turn", "choose", "potion_use", "potion_discard", "proceed", "skip")
    for seed in words:
        count = next(words)
        game = spire.Game()
        game.reset(seed)
        route, act = [], []
        for _ in range(count):
            kind, index, target = next(words), next(words), next(words)
            action = dict(type=names[kind])
            if kind == 0:
                action.update(card=index, target=target)
            elif kind == 2:
                action.update(option=index)
            elif kind in (3, 4):
                action.update(slot=index)
                if kind == 3:
                    action.update(target=target)
            route.append(game.legal_actions().index(action))
            act.append(game.snapshot()["run"]["actNum"])
            game.step(action)
        assert not game.legal_actions()
        seeds.append(seed)
        routes.append(route)
        acts.append(act)
    width = max(map(len, routes)) + 1
    candidates = np.zeros((len(routes), width), dtype=np.int32)
    act_ids = np.zeros((len(routes), width), dtype=np.uint8)
    for i, (route, act) in enumerate(zip(routes, acts)):
        candidates[i, :len(route)] = route
        act_ids[i, :len(act)] = act
    return seeds, candidates, act_ids, np.array(list(map(len, routes)))


def measure(threads, size, batches, warmup, seconds, deep=None):
    roots = list(range(1000000000, 1000010000)) if deep is None else deep[0]
    # Repeated caller-supplied roots bound the benchmark workload; no seed generation in workers.
    seeds = roots * (200000 // len(roots) + 1)
    supplied = len(seeds)
    batch = spire.TrainingBatch(size, threads, 3000, seeds)
    counts, faults, advanced = batch.candidate_counts, batch.faults, batch.advanced
    timing, done, resets, truncated = batch.timing_ns, batch.done, batch.reset_mask, batch.truncated
    seed_indices, decision_counts = batch.seed_indices, batch.decision_counts
    rng = np.random.default_rng(1729)
    selected = np.empty(size, dtype=np.int32)

    def select():
        if deep is None:
            selected[:] = rng.integers(np.maximum(counts, 1), dtype=np.int32)
        else:
            selected[:] = deep[1][seed_indices % len(roots), decision_counts]

    for _ in range(warmup):
        select()
        batch.step(selected)
        assert not faults.any(), faults
    before = timing.sum(axis=0)
    decisions = terminals = reset_count = caps = iterations = 0
    act_counts = np.zeros(5, dtype=np.int64)
    start = time.perf_counter()
    minimum_batches = max(batches, deep[1].shape[1] - warmup) if deep is not None else batches
    while iterations < minimum_batches or time.perf_counter() - start < seconds:
        if int(seed_indices.max()) + size >= supplied:
            batch.extend_seeds(seeds)
            supplied += len(seeds)
        select()
        if deep is not None:
            current_acts = deep[2][seed_indices % len(roots), decision_counts]
        batch.step(selected)
        assert not faults.any(), faults
        decisions += int(advanced.sum())
        terminals += int(done.sum())
        reset_count += int(resets.sum())
        caps += int(truncated.sum())
        if deep is not None:
            act_counts += np.bincount(current_acts[advanced != 0], minlength=5)
            assert (decision_counts[done != 0] == deep[3][seed_indices[done != 0] % len(roots)]).all()
        iterations += 1
    elapsed = time.perf_counter() - start
    reset_ns, step_ns, encode_ns = map(int, timing.sum(axis=0) - before)
    measured = reset_ns + step_ns + encode_ns
    return dict(mode="random" if deep is None else "deep", threads=threads, environments=size,
                batches=iterations, decisions=decisions, seconds=elapsed,
                decisions_per_second=decisions / elapsed, terminal_episodes=terminals,
                resets=reset_count, truncated=caps, faults=0, observation_bytes=spire.observation_bytes,
                game_bytes=spire.game_bytes, worker_reset_ns=reset_ns, worker_step_ns=step_ns,
                worker_encode_ns=encode_ns, encode_share=encode_ns / measured,
                step_share=step_ns / measured, reset_share=reset_ns / measured,
                encode_vs_step_share=encode_ns / (encode_ns + step_ns),
                decisions_by_act=act_counts[1:].tolist() if deep is not None else None)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--batches", type=int, default=400)
    parser.add_argument("--seconds", type=float, default=5)
    parser.add_argument("--warmup", type=int, default=10)
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8])
    parser.add_argument("--mode", choices=("random", "deep", "both"), default="both")
    parser.add_argument("--actions", type=Path, default=Path(__file__).resolve().parents[1] / "tests/deep_benchmark.actions")
    args = parser.parse_args()
    print(json.dumps(dict(machine=subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip(),
                          platform=platform.platform(), policy="numpy PCG64(1729) uniform candidates / accepted deep routes")), flush=True)
    if args.mode in ("random", "both"):
        for threads in args.threads:
            print(json.dumps(measure(threads, args.size, args.batches, args.warmup, args.seconds)), flush=True)
    if args.mode in ("deep", "both"):
        deep = deep_routes(args.actions)
        for threads in args.threads:
            print(json.dumps(measure(threads, args.size, args.batches, args.warmup, args.seconds, deep)), flush=True)
