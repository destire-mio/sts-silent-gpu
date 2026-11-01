"""Run Stage12 no-search PPO, saving rank collection state on yield."""
import argparse
from dataclasses import asdict
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
    save_rank, restore_rank, health_stop, next_learning_rate, decision_entropy_summary, student_log_baseline)
from train.model import ModelConfig
from train.e4_stage6.training_worker import move_tensors
from train.e4_midstart import MidstartCollector


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


def save_dev_weights(policy, output, training_seconds):
    path = output / f"dev-{int(training_seconds // 7200) * 2}.pt"
    if not path.exists():
        torch.save(move_tensors(policy.state_dict(), torch.device("cpu")), path)
    return path.name


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
    common = output / "model.pt"
    saved = torch.load(common, map_location="cpu", weights_only=False) if common.exists() else None
    config = ModelConfig(**saved['state']['model_config']) if saved and 'model_config' in saved['state'] else ModelConfig(256,4,4,1024)
    policy = CLSPolicy(config).to(device)
    model = DistributedDataParallel(policy, device_ids=[local], broadcast_buffers=False)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, eps=1e-5)
    ppo_rng = torch.Generator(device=device).manual_seed(20261031)
    search = NoSLSearch(2048, device, 20261031 + rank, storage_slots=512)
    search.deadline = deadline
    search.evidence_directory = output
    collector = MidstartCollector(search, torch.Generator().manual_seed(20261131 + rank), rank=rank)
    state = dict(update=0, training_seconds=0., initial_entropy=None, next_evaluation=7200.,
                 precision=None, microbatch=None, checkpoint_serial=0, world=world,
                 stop_reason=None, high_kl_streak=0, learning_rate=1e-4)
    cursor = None
    round_state = dict(ready=None, seconds=0., decisions_start=0)
    if common.exists():
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
    teacher = None
    if state.get('teacher_path'):
        teacher = CLSPolicy().to(device)
        teacher.load_state_dict(torch.load(state['teacher_path'], map_location='cpu', weights_only=True))
        teacher.encoder_fp16 = True
        teacher.eval().requires_grad_(False)
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

    def evaluate(evaluated=None, label=None):
        evaluated = evaluated or policy
        gc.collect()
        torch.cuda.empty_cache()
        evaluator_search = NoSLSearch((1024 + world - 1) // world, device, 101 + rank, greedy=True)
        evaluator_search.deadline = deadline
        evaluator_search.evidence_directory = output
        evaluator = NoSLCollector(evaluator_search, torch.Generator().manual_seed(101 + rank), rank=rank)
        evaluated.eval()
        complete = True
        try:
            evaluator.evaluate(evaluated, list(range(2000000000, 2000001024))[rank::world])
        except AllocationEnded:
            complete = False
        episodes = [{k:e[k] for k in ("seed","floor","reach","outcome","reward")} for e in evaluator.completed]
        prefix = label or "evaluation"
        filename = output / f"{prefix}-{state['update']}-rank-{rank}.json.gz"
        with gzip.open(filename, "wt") as stream:
            json.dump(episodes, stream)
        flag = torch.tensor(int(complete), device=device)
        dist.all_reduce(flag, op=dist.ReduceOp.MIN)
        dist.barrier()
        if rank == 0:
            all_episodes = []
            for owner in range(world):
                with gzip.open(output / f"{prefix}-{state['update']}-rank-{owner}.json.gz", "rt") as stream:
                    all_episodes.extend(json.load(stream))
            row = dict(event="teacher_evaluation" if label else "evaluation", selection="argmax", phase="distillation" if state.get("teacher_path") and not state.get("distillation_evaluated") else "PPO", update=state["update"], complete=bool(flag),
                       training_seconds=state["training_seconds"], no_SL=True, **metrics(all_episodes))
            if state.get("teacher_path"):
                row["paired_baseline_model"] = "dev-16.pt argmax"
            if label:
                row["evaluated_model"] = "dev-16.pt"
                row["evaluated_model_training_hours"] = 16
            baseline = output / "baseline.json.gz"
            if state["update"] == 0 and bool(flag):
                with gzip.open(baseline, "wt") as stream:
                    json.dump(all_episodes, stream)
            if baseline.exists():
                with gzip.open(baseline, "rt") as stream:
                    row["paired_baseline"] = paired_summary(all_episodes, json.load(stream))
            if bool(flag) and not label:
                if state.get('distillation_updates') == 80 and not state.get('distillation_evaluated'):
                    name = 'distillation-80.pt'
                    assert not (output / name).exists()
                    torch.save(move_tensors(policy.state_dict(), torch.device('cpu')), output / name)
                    row['model_snapshot'] = name
                else:
                    row["model_snapshot"] = save_dev_weights(policy, output, state["training_seconds"])
            row.update(state.get("decision_entropy_metrics", {}))
            write_json(output / f"{prefix}-{state['update']}.json", row)
            log(row)
        dist.barrier()
        del evaluator, evaluator_search
        gc.collect()
        torch.cuda.empty_cache()
        return bool(flag)

    log(dict(event="start", world=world, lanes_per_gpu=2048, length=64, global_minibatch=32768,
             epochs=2, model_parameters=sum(p.numel() for p in policy.parameters()),
             model=dict(**asdict(config),pooling="CLS"), teacher=state.get("teacher_path"), distillation_updates=state.get("distillation_updates"),
             no_SL=True, from_scratch=not common.exists()))
    last_checkpoint = time.monotonic()
    if not common.exists():
        checkpoint()
    teacher_baseline = output / 'teacher-dev16-argmax-0.json'
    if teacher is not None and (not teacher_baseline.exists() or not json.loads(teacher_baseline.read_text())['complete']):
        if not evaluate(teacher, 'teacher-dev16-argmax'):
            state['stop_reason'] = 'allocation_end_or_yield'
    if teacher is not None and state['distillation_updates'] == 80 and not state.get('distillation_evaluated') and state['stop_reason'] is None:
        if evaluate():
            state['distillation_evaluated'] = True
            state['learning_rate'] = 1e-4
            state['high_kl_streak'] = 0
            checkpoint()
        else:
            state['stop_reason'] = 'allocation_end_or_yield'
    if teacher is not None and state.get('distillation_evaluated'):
        del teacher
        teacher = None
    del saved
    if teacher is None and state["update"] == 0 and not (output / "baseline.json.gz").exists():
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
                search.decision_entropy.zero_()
                search.times = StageTimes()
                search.fault_counts.clear()
                collector.round_games = sum(seed is not None for seed in collector.seeds)
            interrupted = False
            if round_state["ready"] is None:
                policy.eval()
                try:
                    round_state["ready"] = collector.collect(teacher if teacher is not None and state["distillation_updates"] < 80 else policy, length=64)
                except AllocationEnded:
                    interrupted = True
            interrupted = torch.tensor(int(interrupted), device=device)
            dist.all_reduce(interrupted, op=dist.ReduceOp.MAX)
            if bool(interrupted):
                round_state["seconds"] += time.monotonic() - started
                state["stop_reason"] = "allocation_end_or_yield"
                break
            if teacher is not None and state['distillation_updates'] < 80:
                policy.eval()
                student_log_baseline(policy, round_state['ready'])
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
            group["lr"] = state["learning_rate"]
        while cursor["epoch"] < 2:
            if stopping():
                state["stop_reason"] = "allocation_end_or_yield"
                break
            with search.times.measure("learner"):
                advance_minibatch(model, optimizer, scaler, cursor, ppo_rng, rank, world, state["microbatch"],
                    teacher=teacher if teacher is not None and state['distillation_updates'] < 80 else None,
                    entropy_coefficient=state.get('entropy_coefficient', .01))
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
        fresh_done = [e for e in collector.completed if e.get("start_kind", "fresh") == "fresh"]
        middle_done = [e for e in collector.completed if e.get("start_kind", "fresh") != "fresh"]
        done = torch.tensor([len(fresh_done),sum(e["floor"] for e in fresh_done),
                            sum(e["reach"]["act1_boss"] for e in fresh_done),
                            len(middle_done),sum(e["floor"] for e in middle_done)],device=device,dtype=torch.float64)
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
        distilling = teacher is not None and state['distillation_updates'] < 80
        if distilling:
            state['distillation_updates'] += 1
        else:
            state["learning_rate"] = next_learning_rate(state["learning_rate"], row["kl"])
        entropies = search.decision_entropy.clone()
        dist.all_reduce(entropies)
        state["decision_entropy_metrics"] = decision_entropy_summary(entropies)
        if row['normalized_entropy'] < .1:
            log(dict(event='low_normalized_entropy_warning', update=state['update'], normalized_entropy=row['normalized_entropy']))
        if row['kl'] > .1:
            log(dict(event='high_KL_warning', update=state['update'], kl=row['kl'], streak=state['high_kl_streak']))
        log(dict(event="update",phase="distillation" if distilling else "PPO",distillation_updates=state.get("distillation_updates"),update=state["update"],seconds=float(elapsed),training_seconds=state["training_seconds"],
            mainline_decisions_global=int(sum(float(n)for n in by_rank)),decisions_per_second_per_gpu=per_gpu,
            decisions_per_second_global=sum(per_gpu),training_mean_floor=float(done[1]/done[0]) if float(done[0]) else None,
            training_act1_boss_pass_rate=float(done[2]/done[0]) if float(done[0]) else None,
            training_fresh_completed=int(done[0]),training_midstart_completed=int(done[3]),
            training_midstart_mean_floor=float(done[4]/done[3]) if float(done[3]) else None,
            midstart_pool_sizes=[len(p) for p in collector.pools],
            cuda_peak_allocated_bytes=torch.cuda.max_memory_allocated(device),cuda_peak_reserved_bytes=torch.cuda.max_memory_reserved(device),
            timing_seconds=dict(zip(search.times.seconds,parts.tolist())),initial_entropy=state["initial_entropy"],
            precision=state["precision"],microbatch=state["microbatch"],learning_rate=optimizer.param_groups[0]["lr"],
            next_learning_rate=state["learning_rate"],**state["decision_entropy_metrics"],
            stop_reason=state["stop_reason"],episode_faults=faults,episode_cases=games,
            episode_fault_rate=faults/max(1,games),rss_peak_bytes_per_rank=[int(v) for v in rss_by_rank],**row))
        if state.get('model_config', {}).get('width') == 384 and state.get('distillation_updates') == 1 and float(elapsed) > 4 * state['small_round_seconds'] and state['stop_reason'] is None:
            state['architecture_retry_width'] = 320
            state['stop_reason'] = 'allocation_end_or_yield'
            log(dict(event='architecture_retry_required', measured_seconds=float(elapsed), small_seconds=state['small_round_seconds'], next_width=320, reason='more_than_four_times_small_model'))
        cursor = None
        collector.completed.clear()
        round_state = dict(ready=None,seconds=0.,decisions_start=collector.decisions)
        distillation_finished = distilling and state['distillation_updates'] == 80
        if distillation_finished and state['stop_reason'] is None:
            if evaluate():
                state['distillation_evaluated'] = True
                state['learning_rate'] = 1e-4
                state['high_kl_streak'] = 0
                del teacher
                teacher = None
                checkpoint()
                log(dict(event='distillation_complete', update=state['update'], rounds=80, PPO_learning_rate=1e-4))
            else:
                state['stop_reason'] = 'allocation_end_or_yield'
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

