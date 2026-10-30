"""Run Stage12 no-search PPO, saving rank collection state on yield."""
import argparse
from copy import deepcopy
import gc
import gzip
import json
import math
import os
from pathlib import Path
import signal
import resource
import statistics
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.e4 import AllocationEnded, REACH
from train.e4_stage12 import (CLSPolicy, NoSLSearch, NoSLCollector, StageTimes,
    new_cursor, precision_probe, choose_microbatch, advance_minibatch, finish_metrics,
    save_rank, restore_rank, health_stop)
from train.e4_stage6.training_worker import move_tensors


def write_json(path, value):
    temporary = Path(str(path) + ".tmp")
    temporary.write_text(json.dumps(value, allow_nan=False) + "\n")
    temporary.replace(path)


def metrics(episodes):
    count = len(episodes)
    return dict(episodes=count, mean_floor=statistics.mean(e["floor"] for e in episodes) if count else None,
        boss_pass_rates={name:sum(e["reach"][name] for e in episodes) / count if count else None
                         for name in ("act1_boss", "act2_boss", "act3_first_boss")},
        victories=sum(e["outcome"] == "victory" for e in episodes))


def paired_summary(episodes, baseline):
    previous = {e["seed"]:e["floor"] for e in baseline}
    deltas = [e["floor"] - previous[e["seed"]] for e in episodes if e["seed"] in previous]
    return dict(mean_delta=statistics.mean(deltas) if deltas else None,
                paired_SE=statistics.stdev(deltas) / math.sqrt(len(deltas)) if len(deltas) > 1 else None,
                paired_cases=len(deltas))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--deadline", type=float, required=True)
    args = parser.parse_args()
    rank, world, local = (int(os.environ[k]) for k in ("RANK", "WORLD_SIZE", "LOCAL_RANK"))
    torch.set_num_threads(1)
    device = torch.device("cuda", local)
    torch.cuda.set_device(device)
    dist.init_process_group("nccl", device_id=device)
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    stop_file = output / "stop-requested"
    deadline = time.monotonic() + args.deadline - time.time() - 180
    torch.manual_seed(20261031)
    policy = CLSPolicy().to(device)
    model = DistributedDataParallel(policy, device_ids=[local], broadcast_buffers=False)
    optimizer = torch.optim.Adam(model.parameters(), lr=2e-4, eps=1e-5)
    ppo_rng = torch.Generator(device=device).manual_seed(20261031)
    search = NoSLSearch(2048, device, 20261031 + rank)
    search.deadline = deadline
    search.evidence_directory = output
    collector = NoSLCollector(search, torch.Generator().manual_seed(20261131 + rank), rank=rank)
    state = dict(update=0, training_seconds=0., initial_entropy=None, next_evaluation=7200.,
                 precision=None, microbatch=None, checkpoint_serial=0, world=world,
                 stop_reason=None, high_kl_streak=0)
    cursor = None
    round_state = dict(ready=None, seconds=0., decisions_start=0)
    common = output / "model.pt"
    if common.exists():
        saved = torch.load(common, map_location="cpu", weights_only=False)
        policy.load_state_dict(saved["model"])
        optimizer.load_state_dict(saved["optimizer"])
        state.update(saved["state"])
        policy.encoder_fp16 = state["precision"] == "encoder_fp16_heads_fp32"
        ppo_rng.set_state(saved["ppo_rng"])
        rank_path = output / f"rank-{rank}.pt"
        for owner in range(world):
            if owner == rank and rank_path.exists():
                candidate = torch.load(rank_path, map_location="cpu", weights_only=False)
                if candidate["update"] == (state["update"], state["checkpoint_serial"]):
                    cursor, _, round_state = restore_rank(candidate, collector, device, ppo_rng)
                else:
                    print(json.dumps(dict(event="resume_fresh_collector", rank=rank, reason="rank checkpoint differs from shared checkpoint")), flush=True)
                del candidate
            dist.barrier()
        if state["world"] != world:
            cursor = None
            round_state["ready"] = None
            print(json.dumps(dict(event="resume_world_changed", previous=state["world"], current=world,
                                  unfinished_learner="restart collection, retain model and optimizer")), flush=True)
        print(json.dumps(dict(event="resumed", rank=rank, update=state["update"])), flush=True)
    scaler = torch.amp.GradScaler("cuda", enabled=policy.encoder_fp16)
    if common.exists() and state["precision"] == "encoder_fp16_heads_fp32":
        scaler.load_state_dict(saved["scaler"])
    state["world"] = world
    if state["stop_reason"] in ('allocation_end_or_yield', 'entropy_below_half_initial', 'KL_above_0.1'):
        state["stop_reason"] = None
    stopped_by_signal = False
    def on_signal(*_):
        nonlocal stopped_by_signal
        stopped_by_signal = True
    signal.signal(signal.SIGUSR1, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    def log(row):
        if rank == 0:
            print(json.dumps(row, allow_nan=True), flush=True)
            with (output / "training.jsonl").open("a") as stream:
                stream.write(json.dumps(row, allow_nan=True) + "\n")

    def stopping():
        flag = torch.tensor(int(stopped_by_signal or stop_file.exists() or time.monotonic() >= deadline),
                            dtype=torch.int32, device=device)
        dist.all_reduce(flag, op=dist.ReduceOp.MAX)
        return bool(flag)

    def checkpoint():
        state["checkpoint_serial"] += 1
        identity = (state["update"], state["checkpoint_serial"])
        save_rank(output / f"rank-{rank}.pt", collector, cursor, identity, ppo_rng, round_state)
        dist.barrier()
        if rank == 0:
            temporary = output / "model.pt.tmp"
            torch.save(dict(model=move_tensors(policy.state_dict(), torch.device("cpu")),
                            optimizer=move_tensors(optimizer.state_dict(), torch.device("cpu")),
                            scaler=scaler.state_dict(), state=deepcopy(state), ppo_rng=ppo_rng.get_state()), temporary)
            temporary.replace(common)
            log(dict(event="checkpoint", update=state["update"], reason=state["stop_reason"]))
        dist.barrier()

    def evaluate():
        evaluator_search = NoSLSearch((1024 + world - 1) // world, device, 101 + rank)
        evaluator_search.deadline = deadline
        evaluator_search.evidence_directory = output
        evaluator = NoSLCollector(evaluator_search, torch.Generator().manual_seed(101 + rank), rank=rank)
        policy.eval()
        complete = True
        try:
            evaluator.evaluate(policy, list(range(2000000000, 2000001024))[rank::world])
        except AllocationEnded:
            complete = False
        episodes = [{k:e[k] for k in ("seed","floor","reach","outcome","reward")} for e in evaluator.completed]
        filename = output / f"evaluation-{state['update']}-rank-{rank}.json.gz"
        with gzip.open(filename, "wt") as stream:
            json.dump(episodes, stream)
        flag = torch.tensor(int(complete), device=device)
        dist.all_reduce(flag, op=dist.ReduceOp.MIN)
        dist.barrier()
        if rank == 0:
            all_episodes = []
            for owner in range(world):
                with gzip.open(output / f"evaluation-{state['update']}-rank-{owner}.json.gz", "rt") as stream:
                    all_episodes.extend(json.load(stream))
            row = dict(event="evaluation", update=state["update"], complete=bool(flag),
                       training_seconds=state["training_seconds"], no_SL=True, **metrics(all_episodes))
            baseline = output / "baseline.json.gz"
            if state["update"] == 0 and bool(flag):
                with gzip.open(baseline, "wt") as stream:
                    json.dump(all_episodes, stream)
            if baseline.exists():
                with gzip.open(baseline, "rt") as stream:
                    row["paired_baseline"] = paired_summary(all_episodes, json.load(stream))
            write_json(output / f"evaluation-{state['update']}.json", row)
            log(row)
        dist.barrier()
        del evaluator, evaluator_search
        gc.collect()
        torch.cuda.empty_cache()
        return bool(flag)

    log(dict(event="start", world=world, lanes_per_gpu=2048, length=64, global_minibatch=32768,
             epochs=2, model_parameters=sum(p.numel() for p in policy.parameters()),
             model=dict(width=256,layers=4,heads=4,feedforward=1024,pooling="CLS"),
             no_SL=True, from_scratch=not common.exists()))
    last_checkpoint = time.monotonic()
    if not common.exists():
        checkpoint()
    if state["update"] == 0 and not (output / "baseline.json.gz").exists():
        if not evaluate():
            state["stop_reason"] = "allocation_end_or_yield"
    while state["stop_reason"] is None:
        if stopping():
            state["stop_reason"] = "allocation_end_or_yield"
            break
        started = time.monotonic()
        if cursor is None:
            if collector.partial is None and round_state["ready"] is None:
                round_state = dict(ready=None, seconds=0., decisions_start=collector.decisions)
                search.entropy_sum.zero_()
                search.entropy_count = 0
                search.times = StageTimes()
                search.fault_counts.clear()
                collector.round_games = sum(seed is not None for seed in collector.seeds)
            interrupted = False
            if round_state["ready"] is None:
                policy.eval()
                try:
                    round_state["ready"] = collector.collect(policy, length=64)
                except AllocationEnded:
                    interrupted = True
            interrupted = torch.tensor(int(interrupted), device=device)
            dist.all_reduce(interrupted, op=dist.ReduceOp.MAX)
            if bool(interrupted):
                round_state["seconds"] += time.monotonic() - started
                state["stop_reason"] = "allocation_end_or_yield"
                break
            cursor = new_cursor(round_state["ready"], ppo_rng, world)
            round_state["ready"] = None
            if not sum(cursor["counts"]):
                failed = torch.tensor([sum(search.fault_counts.values()), collector.round_games], device=device, dtype=torch.int64)
                dist.all_reduce(failed)
                faults, games = failed.tolist()
                state['stop_reason'] = health_stop(state, dict(kl=0., normalized_entropy=1., finite=True,
                    amp_scale=scaler.get_scale(), amp_skipped_steps=0, optimizer_attempts=0), faults, games)
                log(dict(event="no_valid_rows", update=state["update"], episode_faults=faults,
                         episode_cases=games, stop_reason=state['stop_reason']))
                cursor = None
                continue
            if state["initial_entropy"] is None:
                ent = torch.stack((search.entropy_sum, search.entropy_sum.new_tensor(search.entropy_count)))
                dist.all_reduce(ent)
                state["initial_entropy"] = float(ent[0] / ent[1].clamp_min(1))
            if state["precision"] is None:
                probe = precision_probe(policy, cursor, rank)
                state["precision"] = probe["precision"]
                scaler = torch.amp.GradScaler("cuda", enabled=policy.encoder_fp16)
                log(dict(event="precision_probe", update=state["update"], **probe))
            if state["microbatch"] is None:
                model.train()
                state["microbatch"] = choose_microbatch(model, cursor)
                log(dict(event="microbatch", value=state["microbatch"]))
        model.train()
        for group in optimizer.param_groups:
            group["lr"] = 2e-4 * min(1., (state["update"] + 1) / 20)
        while cursor["epoch"] < 2:
            if stopping():
                state["stop_reason"] = "allocation_end_or_yield"
                break
            with search.times.measure("learner"):
                advance_minibatch(model, optimizer, scaler, cursor, ppo_rng, rank, world, state["microbatch"])
        sync_started = time.perf_counter()
        torch.cuda.synchronize(device)
        search.times.seconds["learner"] += time.perf_counter() - sync_started
        round_state["seconds"] += time.monotonic() - started
        if state["stop_reason"] is not None:
            break
        row = finish_metrics(cursor, model, scaler)
        elapsed = torch.tensor(round_state["seconds"], dtype=torch.float64, device=device)
        dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
        decisions = torch.tensor(collector.decisions - round_state["decisions_start"], device=device, dtype=torch.float64)
        by_rank = [torch.empty_like(decisions) for _ in range(world)]
        dist.all_gather(by_rank, decisions)
        done = torch.tensor([len(collector.completed),sum(e["floor"] for e in collector.completed),
                            sum(e["reach"]["act1_boss"] for e in collector.completed)],device=device,dtype=torch.float64)
        dist.all_reduce(done)
        parts = torch.tensor(list(search.times.seconds.values()),device=device,dtype=torch.float64)
        dist.all_reduce(parts, op=dist.ReduceOp.MAX)
        per_gpu = [float(n) / max(.001,float(elapsed)) for n in by_rank]
        state["update"] += 1
        state["training_seconds"] += float(elapsed)
        health = torch.tensor([sum(search.fault_counts.values()), collector.round_games], device=device, dtype=torch.int64)
        dist.all_reduce(health)
        faults, games = health.tolist()
        rss = torch.tensor(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024, device=device, dtype=torch.int64)
        rss_by_rank = [torch.empty_like(rss) for _ in range(world)]
        dist.all_gather(rss_by_rank, rss)
        state['stop_reason'] = health_stop(state, row, faults, games)
        if row['normalized_entropy'] < .1:
            log(dict(event='low_normalized_entropy_warning', update=state['update'], normalized_entropy=row['normalized_entropy']))
        if row['kl'] > .1:
            log(dict(event='high_KL_warning', update=state['update'], kl=row['kl'], streak=state['high_kl_streak']))
        log(dict(event="update",update=state["update"],seconds=float(elapsed),training_seconds=state["training_seconds"],
            mainline_decisions_global=int(sum(float(n)for n in by_rank)),decisions_per_second_per_gpu=per_gpu,
            decisions_per_second_global=sum(per_gpu),training_mean_floor=float(done[1]/done[0]) if float(done[0]) else None,
            training_act1_boss_pass_rate=float(done[2]/done[0]) if float(done[0]) else None,
            timing_seconds=dict(zip(search.times.seconds,parts.tolist())),initial_entropy=state["initial_entropy"],
            precision=state["precision"],microbatch=state["microbatch"],learning_rate=optimizer.param_groups[0]["lr"],
            stop_reason=state["stop_reason"],episode_faults=faults,episode_cases=games,
            episode_fault_rate=faults/max(1,games),rss_peak_bytes_per_rank=[int(v) for v in rss_by_rank],**row))
        cursor = None
        collector.completed.clear()
        round_state = dict(ready=None,seconds=0.,decisions_start=collector.decisions)
        if state["training_seconds"] >= state["next_evaluation"] and state["stop_reason"] is None:
            if evaluate():
                state["next_evaluation"] += 7200.
            else:
                state["stop_reason"] = "allocation_end_or_yield"
        if time.monotonic() - last_checkpoint >= 1800 and state["stop_reason"] is None:
            checkpoint()
            last_checkpoint = time.monotonic()
    checkpoint()
    log(dict(event="stopped",update=state["update"],reason=state["stop_reason"],training_seconds=state["training_seconds"]))
    dist.destroy_process_group()


if __name__ == "__main__":
    main()

