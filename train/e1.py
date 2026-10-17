import argparse
import collections
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import resource
import time

import numpy as np
import torch

from tools.e2_roots import partition
from .evaluate import compare, evaluate
from .metrics import summarize
from .model import ModelConfig, Policy
from .ppo import PPOConfig, update
from .rollout import Collector
from .roots import read_roots, validate_training_roots
from .run import engine_commit, save_checkpoint


def family_groups(roots):
    groups = collections.defaultdict(list)
    for root in roots:
        groups[root["seed"]].append(root)
    return [groups[seed] for seed in sorted(groups)]


def sample_roots(groups, count, rng):
    return [family[int(rng.integers(len(family)))]
            for family in (groups[int(i)] for i in rng.integers(len(groups), size=count))]


def evaluation_sets(roots):
    sets = {"all": roots}
    for distance in (8, 32, 128):
        group = [r for r in roots if r["distance_to_win"] == distance]
        if group:
            sets[str(distance)] = group
    return {name: sample_roots(family_groups(group), 200, np.random.default_rng(62000 +
            (0 if name == "all" else int(name)))) for name, group in sets.items()}


def main():
    parser = argparse.ArgumentParser(description="Fixed-root E1 terminal-reward experiment")
    parser.add_argument("--roots", type=Path, required=True)
    parser.add_argument("--dev-roots", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--init-seed", type=int, required=True)
    parser.add_argument("--torch-threads", type=int, default=4)
    parser.add_argument("--decisions", type=int, default=200000)
    parser.add_argument("--deadline-epoch", type=float, required=True)
    args = parser.parse_args()
    torch.set_num_threads(args.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(args.init_seed)
    device = torch.device("cpu")
    model_config, ppo_config = ModelConfig(), PPOConfig()
    model = Policy(model_config)
    optimizer = torch.optim.Adam(model.parameters(), lr=ppo_config.learning_rate)
    roots, dev = read_roots(args.roots), read_roots(args.dev_roots)
    validate_training_roots(roots)
    assert all(partition(root["seed"]) == "dev" for root in dev)
    assert not {root["seed"] for root in roots}.intersection(root["seed"] for root in dev)
    groups = family_groups(roots)
    schedules = {"train": evaluation_sets(roots), "dev": evaluation_sets(dev) if dev else {}}
    args.output.mkdir(parents=True, exist_ok=False)
    config = dict(model=asdict(model_config), ppo=asdict(ppo_config), observation_version=2, memory="none",
                  engine_commit=engine_commit(), torch_version=str(torch.__version__),
                  parameters=sum(p.numel() for p in model.parameters()),
                  run={name: str(value) if isinstance(value, Path) else value for name, value in vars(args).items()},
                  root_hashes={str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in (args.roots, args.dev_roots)},
                  train_sampling="uniform family, uniform available exact-distance root within family",
                  policy_sampling="PCG64(init_seed, seed low32, seed high32, episode ordinal), inverse CDF",
                  evaluation="200 independent policy attempts per set; fixed root mixture and RNG before/after",
                  decision_cap=4096, batch_size=32, episodes_per_update=48, storage_mib=256,
                  truncation="stop; E1 requires complete terminal Monte-Carlo targets", faults="stop and retain trajectory")
    (args.output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    (args.output / "evaluation-roots.json").write_text(json.dumps(schedules) + "\n")
    save_checkpoint(args.output / "initial.pt", model, optimizer, config, 0)
    collector = Collector(32, 4096, 256, device, 1)
    sampling_rng = np.random.default_rng(np.random.SeedSequence([args.init_seed, 1]))
    update_rng = np.random.default_rng(args.init_seed)
    total = episodes = wins = iteration = 0
    next_evaluation = 50000
    started, cpu_started = time.perf_counter(), time.process_time()
    evaluation_decisions = evaluation_replays = 0
    initial = {}

    def assess(label, full):
        nonlocal evaluation_decisions, evaluation_replays
        for part, sets in schedules.items():
            for name, schedule in sets.items():
                if not full and name != "all":
                    continue
                result = evaluate(model, schedule, device, 4096, 32, 62001, list(range(200)))
                result.update(sampling_seed=62001, decision_cap=4096, sampling="PCG64 per seed and attempt",
                              observation_version=2, evaluation_engine_commit=config["engine_commit"],
                              train_decisions=total, partition=part, band=name, label=label)
                if label == "initial":
                    initial[part, name] = result
                else:
                    result["paired"] = compare(initial[part, name], result)
                path = args.output / f"eval-{label}-{part}-{name}.json"
                path.write_text(json.dumps(result, allow_nan=False) + "\n")
                evaluation_decisions += result["decisions"]
                evaluation_replays += sum(e["prefix_decisions"] for e in result["episodes"])
                summary = {key: value for key, value in result.items() if key != "episodes"}
                with (args.output / "learning-curve.jsonl").open("a") as output:
                    output.write(json.dumps(summary, allow_nan=False) + "\n")
                print(json.dumps(dict(event="evaluation", **summary)), flush=True)
                assert result["summary"]["faults"] == 0 and result["summary"]["truncated"] == 0

    assess("initial", True)
    while args.decisions - total >= 4096 and time.time() < args.deadline_epoch - 900:
        count = min(48, (args.decisions - total) // 4096)
        group = sample_roots(groups, count, sampling_rng)
        begin = time.perf_counter()
        rollout = collector.collect(model, group, args.init_seed, range(episodes, episodes + count))
        iteration += 1
        result = summarize(rollout)
        with (args.output / "episodes.jsonl").open("a") as output:
            for episode, root in zip(rollout.episodes, group):
                output.write(json.dumps(dict(update=iteration, distance=root["distance_to_win"],
                                            **{k: v for k, v in episode.items() if k != "records"})) + "\n")
        assert result["faults"] == 0 and result["truncated"] == 0, result
        assert np.array_equal(rollout.returns[:rollout.count], np.round(rollout.returns[:rollout.count]))
        assert ((rollout.returns[:rollout.count] >= 0) & (rollout.returns[:rollout.count] <= 1)).all()
        result["ppo"] = update(model, optimizer, rollout, ppo_config, device, update_rng)
        total += rollout.count
        episodes += count
        wins += result["wins"]
        assert total <= args.decisions
        result.update(update=iteration, cumulative_decisions=total, cumulative_episodes=episodes, cumulative_wins=wins)
        result["timing"] = dict(**rollout.timing, update_seconds=result["ppo"]["update_seconds"],
                               total_seconds=time.perf_counter() - begin)
        result["timing"]["decisions_per_second"] = rollout.count / result["timing"]["total_seconds"]
        with (args.output / "updates.jsonl").open("a") as output:
            output.write(json.dumps(result, allow_nan=False) + "\n")
        save_checkpoint(args.output / "checkpoint.pt", model, optimizer, config, iteration)
        print(json.dumps(dict(event="update", **result)), flush=True)
        if total >= next_evaluation:
            assess(str(next_evaluation), False)
            next_evaluation += 50000
    save_checkpoint(args.output / "checkpoint.pt", model, optimizer, config, iteration)
    assess("final", True)
    result = dict(decisions=total, episodes=episodes, wins=wins, updates=iteration,
                  evaluation_decisions=evaluation_decisions, evaluation_replay_decisions=evaluation_replays,
                  wall_seconds=time.perf_counter() - started, cpu_seconds=time.process_time() - cpu_started,
                  peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                  stop_reason="decision_budget" if args.decisions - total < 4096 else "wall_budget",
                  needs_closer_roots=wins < 100)
    (args.output / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
