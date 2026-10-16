"""Uniform-candidate CPU benchmark including encoding, reset and Python dispatch."""
import argparse
import json
import platform
import subprocess
import time

import numpy as np
import spire


def measure(threads, size, batches, warmup):
    seeds = list(range(1000000000, 1000010000))
    batch = spire.TrainingBatch(size, threads, 3000, seeds)
    counts, faults, advanced = batch.candidate_counts, batch.faults, batch.advanced
    timing, done, resets, truncated = batch.timing_ns, batch.done, batch.reset_mask, batch.truncated
    rng = np.random.default_rng(1729)
    selected = np.empty(size, dtype=np.int32)
    for _ in range(warmup):
        selected[:] = rng.integers(np.maximum(counts, 1), dtype=np.int32)
        batch.step(selected)
        assert not faults.any(), faults
    before = timing.sum(axis=0)
    decisions = terminals = reset_count = caps = 0
    start = time.perf_counter()
    for _ in range(batches):
        selected[:] = rng.integers(np.maximum(counts, 1), dtype=np.int32)
        batch.step(selected)
        assert not faults.any(), faults
        decisions += int(advanced.sum())
        terminals += int(done.sum())
        reset_count += int(resets.sum())
        caps += int(truncated.sum())
    seconds = time.perf_counter() - start
    reset_ns, step_ns, encode_ns = map(int, timing.sum(axis=0) - before)
    measured = reset_ns + step_ns + encode_ns
    return dict(threads=threads, environments=size, decisions=decisions, seconds=seconds,
                decisions_per_second=decisions / seconds, terminal_episodes=terminals,
                resets=reset_count, truncated=caps, faults=0, observation_bytes=spire.observation_bytes,
                game_bytes=spire.game_bytes, worker_reset_ns=reset_ns, worker_step_ns=step_ns,
                worker_encode_ns=encode_ns, encode_share=encode_ns / measured,
                step_share=step_ns / measured, reset_share=reset_ns / measured,
                encode_vs_step_share=encode_ns / (encode_ns + step_ns))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=96)
    parser.add_argument("--batches", type=int, default=300)
    parser.add_argument("--warmup", type=int, default=10)
    args = parser.parse_args()
    print(json.dumps(dict(machine=subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip(),
                          platform=platform.platform(), policy="numpy PCG64(1729), uniform legal candidate index")), flush=True)
    for threads in (1, 4, 8):
        print(json.dumps(measure(threads, args.size, args.batches, args.warmup)), flush=True)
