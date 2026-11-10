"""Score E4 policies on saved gold routes using prefix replay and archived draws."""
import argparse
import json
import math
from pathlib import Path

import torch

from tools.e4_map_labels import (GOLD_DRAWS, LABEL_FORMAT, atomic_json, gold_result, group_roots,
                                read_json, replay, root_mean_se)
from tools.e4_value_truth import collector, load_policy
from train.model import network_inputs


def gold_shards(directory, manifest):
    size, count = manifest["shard_size"], manifest["roots"]
    for start in range(0, count, size):
        path = directory / f"gold-{start:07d}.json.gz"
        shard = read_json(path)
        if (shard.get("format") != LABEL_FORMAT or shard.get("mode") != "gold"
                or len(shard["rows"]) != min(size, count - start)):
            raise ValueError(f"Unexpected gold shard {path}")
        yield shard["rows"]


def score_root(root, action, baseline_action=None):
    options = root["options"]
    archived = {int(key): values for key, values in root["scores"].items()}
    if (set(options) != archived.keys() or len(root["draws"]) != GOLD_DRAWS
            or any(len(values) != GOLD_DRAWS or any(v is not None and not math.isfinite(v) for v in values)
                   for values in archived.values())):
        raise ValueError(f"Invalid gold draws for seed {root['seed']} at prefix {len(root['path'])}")
    # Preserve the archive's option order for ties in the first 64 draws.
    scores = {option: archived[option] for option in options}
    result = gold_result(scores, action)
    row = dict(seed=root["seed"], act=root["act"], floor=root["floor"], prefix_length=len(root["path"]),
               action=action, oracle_action=result["selected"] if result is not None else None,
               regret=result["regret"] if result is not None else None)
    if baseline_action is not None:
        baseline = gold_result(scores, baseline_action)
        row.update(baseline_action=baseline_action,
                   baseline_regret=baseline["regret"] if baseline is not None else None,
                   regret_minus_baseline=result["regret"] - baseline["regret"] if result is not None else None)
    return row


def route_choices(model, inputs):
    legal = inputs["candidate_mask"]
    logits = model(inputs)[0].float()
    if not bool(torch.isfinite(logits[legal]).all()):
        raise ValueError("Nonfinite policy logit at a gold route")
    return logits.masked_fill(~legal, -torch.inf).argmax(dim=-1).tolist()


@torch.no_grad()
def evaluate(model, baseline, runner, shards):
    search = runner.search
    results = []
    for shard in shards:
        for start in range(0, len(shard), search.lanes):
            records = shard[start:start + search.lanes]
            slots, _ = replay(runner, records)
            inputs = network_inputs(search.arrays, search.device, torch.tensor(slots, device=search.device))
            routes = torch.zeros_like(inputs["candidate_mask"])
            for i, record in enumerate(records):
                routes[i, record["options"]] = True
            inputs["candidate_mask"] &= routes
            actions = route_choices(model, inputs)
            baseline_actions = route_choices(baseline, inputs) if baseline is not None else [None] * len(records)
            results.extend(score_root(root, action, comparison)
                           for root, action, comparison in zip(records, actions, baseline_actions))
    return results


def summarize(rows, with_baseline):
    summaries = {}
    for key, group in sorted(group_roots(rows).items()):
        valid = [row for row in group if row["regret"] is not None]
        metrics = dict(roots=len(group), complete=len(valid),
                       policy=root_mean_se([(row["seed"], row["regret"]) for row in valid]))
        if with_baseline:
            agreed = sum(row["action"] == row["baseline_action"] for row in group)
            metrics.update(baseline=root_mean_se([(row["seed"], row["baseline_regret"]) for row in valid]),
                           regret_minus_baseline=root_mean_se([(row["seed"], row["regret_minus_baseline"])
                                                               for row in valid]),
                           agreements=agreed, choices_compared=len(group), agreement=agreed / len(group))
        summaries[key] = metrics
    return summaries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path, help="Raw Stage12 weights or a map-distill-*.pt snapshot")
    parser.add_argument("--gold", type=Path, required=True, help="Finished gold directory from e4_map_labels")
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", type=Path, help="New JSON file for summary and individual root choices")
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=128)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    if args.lanes < 1:
        parser.error("--lanes must be positive")
    if args.output is not None and args.output.exists():
        parser.error("--output must be a new file")
    manifest = read_json(args.gold / "manifest.json")
    if (manifest.get("format") != LABEL_FORMAT or manifest.get("mode") != "gold"
            or manifest["roots"] < 1 or not 1 <= manifest["shard_size"] <= 512):
        parser.error("--gold must be an E4 gold directory")
    torch.set_num_threads(1)
    device = torch.device(args.device)
    if device.type == "cuda":
        torch.cuda.set_device(device)
    model = load_policy(args.checkpoint, device, args.fp32)
    baseline = load_policy(args.baseline, device, args.fp32) if args.baseline is not None else None
    runner = collector(min(args.lanes, manifest["roots"]), device, manifest["seed"], float("inf"), greedy=True)
    rows = evaluate(model, baseline, runner, gold_shards(args.gold, manifest))
    report = dict(checkpoint=str(args.checkpoint), gold=str(args.gold),
                  baseline=str(args.baseline) if args.baseline is not None else None,
                  reward_units="normalized_current_reward", selection_draws=GOLD_DRAWS // 2,
                  heldout_draws=GOLD_DRAWS // 2, se_unit="root_means_clustered_by_source_seed",
                  prefix_decisions=runner.decisions, summary=summarize(rows, baseline is not None))
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        atomic_json(args.output, dict(report, roots=rows))
    print(json.dumps(report, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
