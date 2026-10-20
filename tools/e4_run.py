import argparse
from collections import deque
from datetime import timedelta
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.checkpoint import engine_commit
from train.e4 import AllocationEnded, REACH, SEARCH_METRICS, RoomSearch, SegmentCollector
from train.e4_learner import update
from train.model import ModelConfig, Policy


def atomic_json(path, value):
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False) + "\n")
    temporary.replace(path)


def atomic_checkpoint(path, value):
    temporary = path.with_suffix(".tmp")
    with temporary.open("wb") as output:
        torch.save(value, output)
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(path)


def queue_waiting():
    rows = subprocess.check_output(["squeue", "-h", "-o", "%u|%T|%b"], text=True).splitlines()
    return [row for row in rows if row.split("|")[0] != os.environ["USER"]
            and row.split("|")[1] == "PENDING"]


def collective_stop(local, device):
    flag = torch.tensor(int(local), device=device)
    dist.all_reduce(flag, op=dist.ReduceOp.MAX)
    return bool(flag)


def summaries(episodes):
    return dict(episodes=len(episodes), average_floor=sum(e["floor"] for e in episodes) / len(episodes),
                reach={key: sum(e["reach"][key] for e in episodes) / len(episodes) for key in REACH},
                victories=sum(e["outcome"] == "victory" for e in episodes),
                stalled=sum(e["outcome"] == "stalled" for e in episodes), faults=0, truncated=0)


def audit(episodes, round_name, version, commit):
    selected = random.Random(version + 1729).sample(episodes, 18)
    return dict(observation_version=2, status="pending_mac_original_replay",
        engine_commit=commit, current_round=round_name,
        rounds=[dict(name=round_name, roots=18, status="pending_mac_original_replay", observation_version=2)],
        roots=[dict(seed=e["seed"], candidates=e["candidates"],
                    partition="dev" if 2000000000 <= e["seed"] <= 2000004095 else "train",
                    round=round_name, source=dict(kind="frozen_policy_evaluation", policy_version=version,
                    outcome=e["outcome"])) for e in selected])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("task", choices=("calibrate", "train"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=int, required=True)
    parser.add_argument("--initialization", type=int, default=1729)
    parser.add_argument("--trials", type=int, default=32)
    parser.add_argument("--lanes", type=int, default=32)
    parser.add_argument("--interim-dev-seeds", type=int, choices=(1024, 4096), default=4096)
    parser.add_argument("--resume", type=Path)
    parser.add_argument("--training-seconds", type=int, required=True)
    args = parser.parse_args()
    rank, world = int(os.environ["RANK"]), int(os.environ["WORLD_SIZE"])
    assert world == 8
    torch.set_num_threads(1)
    device = torch.device("cuda", int(os.environ["LOCAL_RANK"]))
    torch.cuda.set_device(device)
    dist.init_process_group("nccl", timeout=timedelta(minutes=70))
    args.output.mkdir(parents=True, exist_ok=True)
    commit = engine_commit()
    torch.manual_seed(args.initialization)
    policy = Policy(ModelConfig()).to(device)
    ddp = DistributedDataParallel(policy, device_ids=[device.index], broadcast_buffers=False)
    optimizer = torch.optim.Adam(policy.parameters(), lr=3e-4)
    ppo_rng = torch.Generator(device=device).manual_seed(args.initialization + rank)
    search_rng = torch.Generator(device=device).manual_seed(args.initialization + 1000 + rank)
    seeds_rng = torch.Generator().manual_seed(args.initialization + 2000 + rank)
    scratch_slots = max(4096, args.lanes * max(args.trials, 32))
    search = RoomSearch(args.lanes, args.trials, device, search_rng, scratch_slots=scratch_slots)
    search.evidence_directory = args.output
    free_bytes, total_bytes = torch.cuda.mem_get_info(device)
    non_torch_bytes = total_bytes - free_bytes - torch.cuda.memory_reserved(device)
    collector = SegmentCollector(search, seeds_rng, rank=rank)
    version, trained_seconds, next_evaluation = 0, 0., 3600.
    best, stagnant, assessments = dict.fromkeys(REACH, 0.), 0, []
    ready = None
    phase = "calibration" if args.task == "calibrate" else "training"
    calibration_index = 0
    evaluation_state = None
    train_state = None
    evaluation_seconds = 0.
    evaluation_seed_count = 4096
    health_history = deque(maxlen=20)
    if args.resume:
        saved = torch.load(args.resume / f"rank-{rank}.pt", map_location="cpu", weights_only=False)
        assert saved["commit"] == commit and saved["initialization"] == args.initialization
        assert saved["lanes"] == args.lanes and saved["trials"] == args.trials
        assert saved["interim_dev_seeds"] == args.interim_dev_seeds
        policy.load_state_dict(saved["model"])
        optimizer.load_state_dict(saved["optimizer"])
        ppo_rng.set_state(saved["ppo_rng"])
        torch.set_rng_state(saved["torch_rng"])
        torch.cuda.set_rng_state(saved["cuda_rng"], device)
        collector.restore(saved["collector"])
        version, trained_seconds, next_evaluation = saved["version"], saved["trained_seconds"], saved["next_evaluation"]
        best, stagnant, assessments = saved["best"], saved["stagnant"], saved["assessments"]
        ready = saved["ready"]
        if ready:
            ready = {k: v.to(device) for k, v in ready.items()}
        phase, calibration_index = saved["phase"], saved["calibration_index"]
        evaluation_state, train_state = saved["evaluation_state"], saved["train_state"]
        evaluation_seconds = saved["evaluation_seconds"]
        evaluation_seed_count = saved["evaluation_seed_count"]
        search.event_trial_max_steps = saved["event_trial_max_steps"]
        search.metrics = saved["search_metrics"]
        health_history.extend(saved["health_history"])
    allocation_start = time.monotonic()
    search.deadline = allocation_start + args.seconds - 60

    def checkpoint(reason):
        atomic_checkpoint(args.output / f"rank-{rank}.pt", dict(commit=commit,
            initialization=args.initialization, model=policy.state_dict(), optimizer=optimizer.state_dict(),
            ppo_rng=ppo_rng.get_state(), torch_rng=torch.get_rng_state(), cuda_rng=torch.cuda.get_rng_state(device),
            collector=collector.state(), version=version, trained_seconds=trained_seconds,
            next_evaluation=next_evaluation, best=best, stagnant=stagnant, assessments=assessments,
            ready=ready, phase=phase, calibration_index=calibration_index,
            evaluation_state=evaluation_state, train_state=train_state, evaluation_seconds=evaluation_seconds,
            event_trial_max_steps=search.event_trial_max_steps, search_metrics=search.metrics,
            health_history=list(health_history), lanes=args.lanes, trials=args.trials,
            interim_dev_seeds=args.interim_dev_seeds, evaluation_seed_count=evaluation_seed_count))
        dist.barrier()
        if rank == 0:
            atomic_json(args.output / "status.json", dict(reason=reason, task=args.task, phase=phase, version=version,
                initialization=args.initialization, trials=search.trials, lanes=args.lanes,
                scratch_slots=scratch_slots, evaluation_seed_count=evaluation_seed_count,
                trained_seconds=trained_seconds,
                allocation_process_seconds=time.monotonic() - allocation_start,
                job=os.environ["SLURM_JOB_ID"], commit=commit, stagnant=stagnant, assessments=assessments,
                calibration_index=calibration_index, checkpoint_files=[f"rank-{r}.pt" for r in range(world)]))

    try:
        checkpoint("started")
        while True:
            if args.task == "calibrate":
                if calibration_index == 3:
                    completed = [a for a in assessments if a["evaluation_seconds"] <= 1200]
                    chosen = max((a["trials"] for a in completed), default=None)
                    if rank == 0:
                        atomic_json(args.output / "calibration.json", dict(assessments=assessments, selected_trials=chosen))
                    checkpoint("calibration_complete" if chosen else "no_evaluation_within_twenty_minutes")
                    break
                search.trials = (8, 16, 32)[calibration_index]
                phase = "evaluation"
            if phase == "training":
                if version < 20:
                    torch.cuda.reset_peak_memory_stats(device)
                start = time.monotonic()
                actual_before = collector.decisions
                searched_before = search.searched_decisions
                expired = False
                policy.eval()
                try:
                    if ready is None:
                        ready = collector.collect(policy)
                except AllocationEnded:
                    expired = True
                if collective_stop(expired, device):
                    checkpoint("allocation_limit")
                    break
                ddp.train()
                metrics = update(ddp, optimizer, ready, ppo_rng)
                health = ready["health"].clone()
                dist.all_reduce(health[:3], op=dist.ReduceOp.SUM)
                dist.all_reduce(health[3:], op=dist.ReduceOp.MAX)
                completed, floor_sum, stalled, max_floor_steps = health.cpu().tolist()
                health_report = dict(completed_episodes=completed,
                    completed_average_floor=floor_sum / completed if completed else None,
                    stalled_episodes=stalled, active_max_floor_steps=max_floor_steps)
                health_history.append((completed, stalled))
                window_completed = sum(count for count, _ in health_history)
                window_stalled = sum(count for _, count in health_history)
                ready = None
                version += 1
                elapsed = torch.tensor(time.monotonic() - start, device=device)
                dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
                steps = torch.tensor([collector.decisions - actual_before,
                                      search.searched_decisions - searched_before],
                                     dtype=torch.int64, device=device)
                dist.all_reduce(steps, op=dist.ReduceOp.SUM)
                trained_seconds += float(elapsed)
                memory_report = {}
                if version <= 20:
                    peak = torch.tensor([torch.cuda.max_memory_allocated(device),
                        torch.cuda.max_memory_reserved(device),
                        non_torch_bytes + torch.cuda.max_memory_reserved(device)], device=device)
                    dist.all_reduce(peak, op=dist.ReduceOp.MAX)
                    memory_report = dict(peak_torch_allocated_bytes=int(peak[0]),
                        peak_torch_reserved_bytes=int(peak[1]),
                        estimated_peak_device_bytes=int(peak[2]),
                        memory_scope="max_across_ranks; device estimate adds static non-PyTorch allocation")
                if rank == 0:
                    print(json.dumps(dict(event="update", version=version, trained_seconds=trained_seconds,
                                          update_seconds=float(elapsed),
                                          actual_steps=int(steps[0]), sl_trial_steps=int(steps[1]),
                                          actual_steps_per_second=float(steps[0] / elapsed),
                                          sl_trial_steps_per_second=float(steps[1] / elapsed),
                                          initialization=args.initialization, lanes=args.lanes,
                                          trials=search.trials, **memory_report,
                                          **health_report, **metrics)), flush=True)
                if version > 20 and (window_completed == 0
                        or window_completed >= 40 and window_stalled * 20 > window_completed):
                    if rank == 0:
                        atomic_json(args.output / "training-health-stop.json",
                                    dict(version=version, **health_report,
                                         window_updates=len(health_history),
                                         window_completed_episodes=window_completed,
                                         window_stalled_episodes=window_stalled))
                    checkpoint("training_health_stop")
                    break
                if trained_seconds < next_evaluation:
                    continue
                train_state = collector.state()
                evaluation_seed_count = (4096 if trained_seconds >= args.training_seconds
                                         else args.interim_dev_seeds)
                phase = "evaluation"
            if phase == "evaluation":
                dev_seeds = list(range(2000000000 + rank, 2000000000 + evaluation_seed_count, world))
                if evaluation_state is None:
                    search.event_trial_max_steps = 0
                    search.metrics = dict.fromkeys(SEARCH_METRICS, 0)
                checkpoint("before_evaluation")
                waiting = [queue_waiting() if rank == 0 else None]
                dist.broadcast_object_list(waiting, src=0)
                if waiting[0]:
                    checkpoint("yield_to_waiting_user")
                    break
                if evaluation_state is None:
                    collector = SegmentCollector(search, seeds_rng, rank=rank)
                    search_rng.manual_seed(args.initialization + 1000 + rank)
                else:
                    collector.restore(evaluation_state)
                policy.eval()
                start = time.monotonic()
                expired = False
                try:
                    episodes = (collector.completed if evaluation_state is not None
                                and collector.evaluation_waiting is None and len(collector.completed) == len(dev_seeds)
                                else collector.evaluate(policy, dev_seeds))
                except AllocationEnded:
                    episodes = collector.completed
                    expired = True
                elapsed = torch.tensor(time.monotonic() - start, device=device)
                dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
                evaluation_seconds += float(elapsed)
                event_steps = torch.tensor(search.event_trial_max_steps, dtype=torch.int64, device=device)
                dist.all_reduce(event_steps, op=dist.ReduceOp.MAX)
                search.event_trial_max_steps = int(event_steps)
                if collective_stop(expired, device):
                    evaluation_state = collector.state()
                    atomic_json(args.output / f"partial-evaluation-{rank}.json", episodes)
                    checkpoint("allocation_limit_during_evaluation")
                    break
                search_counts = torch.tensor([search.metrics[k] for k in SEARCH_METRICS[:2]], device=device, dtype=torch.int64)
                search_maxima = torch.tensor([search.metrics[k] for k in SEARCH_METRICS[2:]], device=device, dtype=torch.int64)
                dist.all_reduce(search_counts, op=dist.ReduceOp.SUM)
                dist.all_reduce(search_maxima, op=dist.ReduceOp.MAX)
                search_summary = dict(zip(SEARCH_METRICS, search_counts.cpu().tolist() + search_maxima.cpu().tolist()))
                round_name = f"E4-{args.initialization}-{version}-N{search.trials}-D{evaluation_seed_count}"
                atomic_json(args.output / f"{round_name}-rank-{rank}.json", episodes)
                dist.barrier()
                if rank == 0:
                    all_episodes = sum([json.loads((args.output / f"{round_name}-rank-{r}.json").read_text())
                                        for r in range(world)], [])
                    assert len(all_episodes) == evaluation_seed_count
                    assert len({e["seed"] for e in all_episodes}) == evaluation_seed_count
                    summary = dict(summaries(all_episodes), round=round_name, version=version,
                                   trials=search.trials, lanes=args.lanes, dev_seed_count=evaluation_seed_count,
                                   evaluation_seconds=evaluation_seconds,
                                   event_trial_max_steps=search.event_trial_max_steps, **search_summary)
                    atomic_json(args.output / f"{round_name}-summary.json", summary)
                    atomic_json(args.output / f"{round_name}-audit.json", audit(all_episodes, round_name, version, commit))
                    print(json.dumps(dict(event="evaluation", **summary)), flush=True)
                shared = [summary if rank == 0 else None]
                dist.broadcast_object_list(shared, src=0)
                summary = shared[0]
                assessments.append(summary)
                evaluation_state, evaluation_seconds = None, 0.
                if args.task == "calibrate":
                    calibration_index += 1
                    phase = "calibration"
                else:
                    improved = any(summary["reach"][key] - best[key] >= .02 for key in REACH)
                    next_stagnant = 0 if improved else stagnant + 1
                    if next_stagnant >= 3 and evaluation_seed_count < 4096:
                        evaluation_seed_count = 4096
                        checkpoint("before_full_stopping_assessment")
                        continue
                    stagnant = next_stagnant
                    best = {key: max(best[key], summary["reach"][key]) for key in REACH}
                    collector = SegmentCollector(search, seeds_rng, rank=rank)
                    collector.restore(train_state)
                    train_state = None
                    phase = "training"
                    next_evaluation += 3600
                checkpoint("assessment_complete")
                waiting = [queue_waiting() if rank == 0 else None]
                dist.broadcast_object_list(waiting, src=0)
                if waiting[0]:
                    checkpoint("yield_to_waiting_user")
                    break
                if args.task == "train" and stagnant >= 3:
                    checkpoint("three_assessments_without_two_point_improvement")
                    break
                if args.task == "train" and trained_seconds >= args.training_seconds:
                    checkpoint("initialization_training_target")
                    break
                if collective_stop(time.monotonic() >= search.deadline, device):
                    checkpoint("allocation_limit")
                    break
    finally:
        if sys.exc_info()[0] is not None:
            atomic_checkpoint(args.output / f"fault-rank-{rank}.pt", dict(
                commit=commit, version=version, collector=collector.state(),
                model=policy.state_dict(), optimizer=optimizer.state_dict(),
                ppo_rng=ppo_rng.get_state(), phase=phase))
        dist.destroy_process_group()


if __name__ == "__main__":
    main()
