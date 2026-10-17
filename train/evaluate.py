import argparse
import hashlib
import json
from pathlib import Path

import torch

from tools.evaluation import paired_counts, wilson
from .model import ModelConfig, Policy
from .rollout import Collector, DEFAULT_INFERENCE_CHUNK
from .sampling import SCHEME, root_key
from .roots import natural_roots, read_roots
from .checkpoint import engine_commit


def outcome_summary(episodes):
    outcomes = [episode["outcome"] for episode in episodes]
    wins, deaths = outcomes.count("victory"), outcomes.count("death")
    unknown = len(outcomes) - wins - deaths
    return dict(attempts=len(outcomes), wins=wins, deaths=deaths, faults=outcomes.count("fault"),
                truncated=outcomes.count("truncated"),
                independent_root_families_won=len({e["seed"] for e in episodes if e["outcome"] == "victory"}),
                wilson95=wilson(wins, len(outcomes)) if not unknown else None,
                unresolved_win_rate_bounds=[wins / len(outcomes), (wins + unknown) / len(outcomes)],
                wilson95_with_unresolved_bounds=[wilson(wins, len(outcomes))[0], wilson(wins + unknown, len(outcomes))[1]])


def compare(reference, candidate):
    for key in ("sampling_seed", "decision_cap", "sampling", "observation_version", "evaluation_engine_commit"):
        assert reference[key] == candidate[key], f"Paired evaluation differs in {key}"
    old = {(e["root_key"], e["attempt"]): e for e in reference["episodes"]}
    new = {(e["root_key"], e["attempt"]): e for e in candidate["episodes"]}
    assert old.keys() == new.keys()
    assert all(old[seed]["root_key"] == new[seed]["root_key"] for seed in old)
    result = paired_counts({key: e["outcome"] for key, e in old.items()},
                           {key: e["outcome"] for key, e in new.items()})
    result["attempt_pairs"] = result.pop("roots")
    result["root_families"] = len({e["seed"] for e in reference["episodes"]})
    return result


@torch.no_grad()
def evaluate(model, roots, device, decision_cap, batch_size, sampling_seed, attempts=None, *,
             inference_chunk=DEFAULT_INFERENCE_CHUNK):
    assert roots
    if attempts is None:
        assert len({root["seed"] for root in roots}) == len(roots), "Repeated roots require independent attempt IDs"
        attempts = [0] * len(roots)
    assert len(attempts) == len(roots)
    assert len({(root_key(root), attempt) for root, attempt in zip(roots, attempts)}) == len(roots)
    collector = Collector(min(batch_size, len(roots)), decision_cap, 256, device, 1,
                          inference_chunk=inference_chunk)
    rollout = collector.collect(model, roots, sampling_seed, attempts, policy_version=0)
    elapsed = rollout.timing["collection_seconds"]
    return dict(episodes=rollout.episodes, summary=outcome_summary(rollout.episodes), decisions=rollout.count,
                timing=dict(rollout.timing, total_seconds=elapsed, decisions_per_second=rollout.count / elapsed))


def main():
    parser = argparse.ArgumentParser(description="One frozen-policy attempt per seed; paired counter-based streams")
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--roots", type=Path)
    parser.add_argument("--root-index", type=int, help="Select one root, e.g. one distance on the historical E1 route")
    parser.add_argument("--first-seed", type=int, default=2000000000)
    parser.add_argument("--episodes", type=int, default=16)
    parser.add_argument("--partition", choices=("train", "dev", "locked_test"), default="dev")
    parser.add_argument("--sampling-seed", type=int, default=2718)
    parser.add_argument("--decision-cap", type=int, default=3000)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--inference-chunk", type=int, default=DEFAULT_INFERENCE_CHUNK)
    parser.add_argument("--device", default="cuda")
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    device = torch.device(args.device)
    checkpoint = torch.load(args.checkpoint, map_location=device, weights_only=True)
    assert checkpoint["config"]["observation_version"] == 2 and checkpoint["config"]["memory"] == "none"
    model = Policy(ModelConfig(**checkpoint["config"]["model"])).to(device)
    model.load_state_dict(checkpoint["model"])
    roots = read_roots(args.roots) if args.roots else natural_roots(args.first_seed, args.episodes, args.partition)
    if args.root_index is not None:
        roots = [roots[args.root_index]]
    result = evaluate(model, roots, device, args.decision_cap, args.batch_size, args.sampling_seed,
                      inference_chunk=args.inference_chunk)
    result.update(checkpoint=str(args.checkpoint), checkpoint_sha256=hashlib.sha256(args.checkpoint.read_bytes()).hexdigest(),
                  training_engine_commit=checkpoint["engine_commit"], evaluation_engine_commit=engine_commit(),
                  observation_version=2, sampling_seed=args.sampling_seed, decision_cap=args.decision_cap,
                  sampling=SCHEME, roots=roots)
    if args.reference:
        result["paired"] = compare(json.loads(args.reference.read_text()), result)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as output:
        output.write(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key not in ("episodes", "roots")}), flush=True)


if __name__ == "__main__":
    main()
