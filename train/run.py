import argparse
from dataclasses import asdict
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np
import torch

from .metrics import summarize
from .model import ModelConfig, Policy
from .ppo import PPOConfig, update
from .rollout import Collector
from .roots import natural_roots, read_roots, validate_training_roots


def engine_commit():
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=Path(__file__).resolve().parents[1], text=True).strip()


def save_checkpoint(path, model, optimizer, config, iteration):
    torch.save(dict(model=model.state_dict(), optimizer=optimizer.state_dict(), config=config, update=iteration,
                    engine_commit=config["engine_commit"], torch_rng=torch.get_rng_state()), path)


def main():
    parser = argparse.ArgumentParser(description="E1 no-memory synchronous PPO; terminal gamma=1 Monte-Carlo targets")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--roots", type=Path)
    parser.add_argument("--first-seed", type=int, default=1000000000)
    parser.add_argument("--updates", type=int, default=2)
    parser.add_argument("--episodes", type=int, default=16, help="Fixed number of episode starts per update")
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--decision-cap", type=int, default=3000)
    parser.add_argument("--storage-mib", type=int, default=256)
    parser.add_argument("--init-seed", type=int, default=1729)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--torch-threads", type=int, default=4)
    parser.add_argument("--width", type=int, default=128)
    parser.add_argument("--layers", type=int, default=4)
    parser.add_argument("--heads", type=int, default=4)
    parser.add_argument("--feedforward", type=int, default=256)
    parser.add_argument("--learning-rate", type=float, default=0.0003)
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--minibatch-size", type=int, default=32)
    parser.add_argument("--gae-lambda", type=float, default=1.0)
    parser.add_argument("--entropy-coefficient", type=float, default=0.01)
    args = parser.parse_args()
    assert args.updates > 0 and args.episodes > 0 and args.batch_size > 0 and args.decision_cap > 0
    assert args.epochs > 0 and args.minibatch_size > 0 and 0 <= args.gae_lambda <= 1
    assert args.torch_threads > 0
    torch.set_num_threads(args.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(args.init_seed)
    device = torch.device(args.device)
    model_config = ModelConfig(args.width, args.layers, args.heads, args.feedforward)
    ppo_config = PPOConfig(learning_rate=args.learning_rate, epochs=args.epochs, minibatch_size=args.minibatch_size,
                           gae_lambda=args.gae_lambda, entropy_coefficient=args.entropy_coefficient)
    model = Policy(model_config).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=ppo_config.learning_rate)
    rng = np.random.default_rng(args.init_seed)
    roots = read_roots(args.roots) if args.roots else natural_roots(args.first_seed, args.updates * args.episodes, "train")
    validate_training_roots(roots)
    args.output.mkdir(parents=True, exist_ok=False)
    config = dict(model=asdict(model_config), ppo=asdict(ppo_config), observation_version=2, memory="none",
                  engine_commit=engine_commit(), torch_version=str(torch.__version__),
                  parameters=sum(p.numel() for p in model.parameters()),
                  run={name: str(value) if isinstance(value, Path) else value for name, value in vars(args).items()},
                  sampling="PCG64 SeedSequence(init_seed, root_seed low32, root_seed high32, episode ordinal); inverse CDF",
                  truncation="bootstrap frozen final-observation value; exclude from win/loss calibration",
                  faults="exclude entire trajectory from PPO; retain outcome and candidate prefix")
    (args.output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    (args.output / "roots.json").write_text(json.dumps(dict(observation_version=2, roots=roots)) + "\n")
    save_checkpoint(args.output / "initial.pt", model, optimizer, config, 0)
    print(json.dumps(dict(config=config)), flush=True)
    collector = Collector(min(args.batch_size, args.episodes), args.decision_cap, args.storage_mib, device, args.gae_lambda)
    for iteration in range(1, args.updates + 1):
        start = time.perf_counter()
        first = (iteration - 1) * args.episodes
        group = [roots[i % len(roots)] for i in range(first, first + args.episodes)]
        rollout = collector.collect(model, group, args.init_seed, range(first, first + args.episodes), policy_version=iteration - 1)
        result = summarize(rollout)
        result["ppo"] = update(model, optimizer, rollout, ppo_config, device, rng, policy_version=iteration - 1)
        result.update(update=iteration, engine_commit=config["engine_commit"])
        result["timing"] = dict(**rollout.timing, update_seconds=result["ppo"]["update_seconds"])
        save_checkpoint(args.output / "checkpoint.pt", model, optimizer, config, iteration)
        with (args.output / "episodes.jsonl").open("a") as output:
            for episode in rollout.episodes:
                output.write(json.dumps(dict(update=iteration, **{k: v for k, v in episode.items() if k != "records"})) + "\n")
        elapsed = time.perf_counter() - start
        result["timing"]["total_seconds"] = elapsed
        result["timing"]["decisions_per_second"] = rollout.count / elapsed
        result["timing"]["other_seconds"] = elapsed - sum(result["timing"][key] for key in
                                                             ("engine_encode_seconds", "inference_seconds", "update_seconds", "storage_seconds"))
        peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        result["peak_rss_mib"] = peak / (1024 * 1024 if sys.platform == "darwin" else 1024)
        line = json.dumps(result, allow_nan=False)
        with (args.output / "updates.jsonl").open("a") as output:
            output.write(line + "\n")
        print(line, flush=True)


if __name__ == "__main__":
    main()
