import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import resource
import sys
import time

import torch

from .checkpoint import engine_commit
from .metrics import summarize
from .model import ModelConfig, Policy
from .pipeline import Pipeline, PipelineConfig
from .ppo import PPOConfig
from .sampling import SCHEME
from .rollout import DEFAULT_INFERENCE_CHUNK
from .roots import natural_roots, read_roots, validate_training_roots


def main():
    parser = argparse.ArgumentParser(description="Device-local collection and synchronous single-node DDP PPO")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--resume", type=Path, help="Resume a train.run checkpoint into a fresh output directory")
    parser.add_argument("--roots", type=Path)
    parser.add_argument("--first-seed", type=int, default=1000000000)
    parser.add_argument("--updates", type=int, help="Total update target; defaults to 2 or the resumed run's target")
    parser.add_argument("--episodes", type=int, default=16, help="Fixed number of episode starts per update")
    parser.add_argument("--batch-size", type=int, default=8, help="Environment lanes per GPU")
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--decision-cap", type=int, default=3000)
    parser.add_argument("--storage-mib", type=int, default=256)
    parser.add_argument("--inference-chunk", type=int, default=DEFAULT_INFERENCE_CHUNK)
    parser.add_argument("--init-seed", type=int, default=1729)
    parser.add_argument("--device", choices=("cpu", "cuda"), default="cuda")
    parser.add_argument("--torch-threads", type=int, default=1)
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
    checkpoint = None
    commit = engine_commit()
    if args.resume is not None:
        checkpoint = torch.load(args.resume, map_location="cpu", weights_only=True)
        previous = checkpoint["config"]
        assert checkpoint["format_version"] == 1 and previous["entrypoint"] == "train.run"
        assert previous["engine_commit"] == commit, "Resume requires the checkpoint's engine revision"
        assert previous["torch_version"] == str(torch.__version__), "Resume requires the checkpoint's PyTorch version"
        parser.set_defaults(**previous["run"])
        parser.set_defaults(output=args.output, resume=args.resume)
        args = parser.parse_args()
        args.resume = args.resume.resolve()
    elif args.updates is None:
        args.updates = 2
    run = {name: str(value) if isinstance(value, Path) else value for name, value in vars(args).items()}
    if checkpoint is not None:
        fixed = set(run) - {"output", "resume", "updates"}
        assert {k: run[k] for k in fixed} == {k: previous["run"][k] for k in fixed}, \
            "Resume preserves training arguments; only --output and --updates may change"
        assert args.updates > checkpoint["policy_version"], "Update target is already complete"
    assert args.updates > 0 and args.episodes > 0 and args.batch_size > 0 and args.decision_cap > 0
    assert args.epochs > 0 and args.minibatch_size > 0 and 0 <= args.gae_lambda <= 1
    assert args.torch_threads > 0
    torch.set_num_threads(args.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(args.init_seed)
    model_config = ModelConfig(args.width, args.layers, args.heads, args.feedforward)
    ppo_config = PPOConfig(learning_rate=args.learning_rate, epochs=args.epochs, minibatch_size=args.minibatch_size,
                           gae_lambda=args.gae_lambda, entropy_coefficient=args.entropy_coefficient)
    pipeline_config = PipelineConfig(args.workers, args.batch_size, args.decision_cap,
                                     args.storage_mib, args.device, args.torch_threads, args.inference_chunk)
    if checkpoint is not None:
        roots = read_roots(args.resume.parent / "roots.json")
    else:
        roots = read_roots(args.roots) if args.roots else natural_roots(args.first_seed, args.updates * args.episodes, "train")
    validate_training_roots(roots)
    assert roots
    roots_text = json.dumps(dict(observation_version=2, roots=roots)) + "\n"
    roots_sha256 = hashlib.sha256(roots_text.encode()).hexdigest()
    if checkpoint is not None:
        assert roots_sha256 == previous["roots_sha256"], "Resume roots differ from the checkpoint"
    args.output.mkdir(parents=True, exist_ok=False)
    config = dict(entrypoint="train.run", model=asdict(model_config), ppo=asdict(ppo_config), observation_version=2, memory="none",
                  engine_commit=commit, torch_version=str(torch.__version__), roots_sha256=roots_sha256,
                  parameters=sum(p.numel() for p in Policy(model_config).parameters()), pipeline=asdict(pipeline_config),
                  run=run,
                  sampling=SCHEME,
                  truncation="bootstrap frozen final-observation value; exclude from win/loss calibration",
                  faults="exclude entire trajectory from PPO; retain outcome and candidate prefix",
                  policy_lag=0, policy_versions="collect v locally, DDP update v, barrier, publish v+1",
                  minibatch="global decisions across all learner ranks; normalize advantages over the complete rollout",
                  cpu_schedule="at most three test workers")
    (args.output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    (args.output / "roots.json").write_text(roots_text)
    print(json.dumps(dict(config=config)), flush=True)
    with Pipeline(model_config, ppo_config, pipeline_config, args.init_seed, args.output, config, checkpoint) as pipeline:
        started = time.perf_counter()
        pipeline.initialize()
        print(json.dumps(dict(startup_seconds=time.perf_counter() - started)), flush=True)
        for iteration in range(pipeline.version + 1, args.updates + 1):
            start = time.perf_counter()
            first = pipeline.next_attempt
            group = [roots[i % len(roots)] for i in range(first, first + args.episodes)]
            rollout = pipeline.collect(group, args.init_seed, range(first, first + args.episodes))
            result = summarize(rollout)
            result["ppo"] = pipeline.update(rollout)
            result.update(update=iteration, engine_commit=config["engine_commit"])
            result["timing"] = dict(**rollout.timing, update_seconds=result["ppo"]["update_seconds"],
                                    learner_seconds=result["ppo"]["learner_seconds"])
            with (args.output / "episodes.jsonl").open("a") as output:
                for episode in rollout.episodes:
                    output.write(json.dumps(dict(update=iteration, **{k: v for k, v in episode.items() if k != "records"})) + "\n")
            elapsed = time.perf_counter() - start
            result["timing"]["total_seconds"] = elapsed
            result["timing"]["decisions_per_second"] = rollout.count / elapsed
            result["timing"]["other_seconds"] = elapsed - rollout.timing["collection_seconds"] - result["ppo"]["learner_seconds"]
            peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            result["coordinator_peak_rss_mib"] = peak / (1024 * 1024 if sys.platform == "darwin" else 1024)
            line = json.dumps(result, allow_nan=False)
            with (args.output / "updates.jsonl").open("a") as output:
                output.write(line + "\n")
            print(line, flush=True)


if __name__ == "__main__":
    main()
