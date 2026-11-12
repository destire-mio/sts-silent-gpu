"""Fit only a RouteHead to E4 route labels with a source-seed holdout."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import random
from tempfile import NamedTemporaryFile

import torch

from tools.e4_map_gold_eval import gold_shards
from tools.e4_map_labels import LABEL_FORMAT, read_json
from tools.e4_value_truth import load_policy
from train.e4_map_distill import (HEAD_LEARNING_RATE, LABELS_PER_UPDATE, MapDistillPolicy, batch_targets,
                                  read_label_rows, route_head_objective, target_rows, train_head_step)
from train.e4_stage6.training_worker import move_tensors


PUBLISHED_GOLD_SOURCE_SEEDS = range(1000000000, 1000002048)
SPLIT_SEED = 20261111


def gold_source_seeds(directory):
    if directory is None:
        return set(PUBLISHED_GOLD_SOURCE_SEEDS), "published_gold_source_pool"
    manifest = read_json(directory / "manifest.json")
    if (manifest.get("format") != LABEL_FORMAT or manifest.get("mode") != "gold"
            or manifest["roots"] < 1 or not 1 <= manifest["shard_size"] <= 512):
        raise ValueError("Expected a finished E4 gold directory")
    return {row["seed"] for shard in gold_shards(directory, manifest) for row in shard}, "archived_gold_roots"


def split_by_seed(rows, excluded):
    allowed = [row for row in rows if row["seed"] not in excluded]
    acts_by_seed = defaultdict(set)
    for row in allowed:
        acts_by_seed[row["seed"]].add(row["act"])
    seeds = sorted(acts_by_seed)
    if len(seeds) < 2:
        raise ValueError("At least two non-gold label source seeds are required")
    random.Random(SPLIT_SEED).shuffle(seeds)
    count = max(1, round(len(seeds) / 10))
    train_seeds_per_act = Counter(act for acts in acts_by_seed.values() for act in acts)
    held_out = set()
    for seed in seeds:
        acts = acts_by_seed[seed]
        if all(train_seeds_per_act[act] > 1 for act in acts):
            held_out.add(seed)
            train_seeds_per_act.subtract(acts)
            if len(held_out) == count:
                break
    if len(held_out) != count:
        raise ValueError("Cannot hold out 10% of source seeds while keeping each act represented in training")
    return ([row for row in allowed if row["seed"] not in held_out],
            [row for row in allowed if row["seed"] in held_out])


@torch.no_grad()
def validation_metrics(policy, rows, targets, device):
    label_best = [max(row["estimates"], key=lambda option: option["mean"])["action"] for row in rows]
    total_kl = total_head_abs = options = agreements = 0
    for start in range(0, len(targets), LABELS_PER_UPDATE):
        batch = targets[start:start + LABELS_PER_UPDATE]
        inputs, teacher = batch_targets(batch, device)
        kl, student, residual, _ = route_head_objective(policy, inputs, teacher)
        selected = student.argmax(-1).tolist()
        agreements += sum(a == b for a, b in zip(selected, label_best[start:start + len(batch)]))
        legal = inputs["candidate_mask"]
        total_kl += float(kl) * len(batch)
        total_head_abs += float(residual[legal].abs().sum())
        options += int(legal.sum())
    return dict(validation_kl=total_kl / len(rows), head_mean_abs=total_head_abs / options,
                validation_label_best_agreement=agreements / len(rows))


def save_snapshot(policy, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    snapshot = dict(format="e4_map_distill_policy_v1", model=policy.state_dict(),
                    map_head=policy.route_head.state_dict(), map_weight=policy.map_weight)
    with NamedTemporaryFile(dir=output.parent, prefix=f".{output.name}.", suffix=".tmp", delete=False) as stream:
        temporary = Path(stream.name)
    try:
        torch.save(move_tensors(snapshot, torch.device("cpu")), temporary)
        if output.exists():
            raise FileExistsError(output)
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path, help="Stage12 policy weights such as dev-112.pt")
    parser.add_argument("--labels", type=Path, required=True, help="Finished E4 map label directory")
    parser.add_argument("--gold", type=Path, help="Finished gold directory to audit instead of the published source range")
    parser.add_argument("--epochs", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True, help="New map-distill snapshot")
    parser.add_argument("--device", default="cuda:0")
    args = parser.parse_args()
    if args.epochs < 1 or args.output.exists():
        parser.error("--epochs must be positive and --output must be a new file")

    rows, fingerprint, label_seeds = read_label_rows(args.labels)
    if None in label_seeds:
        raise ValueError("Map label rows must include their source seed")
    gold_seeds, gold_scope = gold_source_seeds(args.gold)
    overlap = label_seeds & gold_seeds
    train_rows, valid_rows = split_by_seed(rows, overlap)
    train_targets, variances = target_rows(train_rows)
    valid_targets, _ = target_rows(valid_rows, variances=variances)
    print(json.dumps(dict(event="map_offline_split", labels_sha256=fingerprint, gold_seed_scope=gold_scope,
                          label_source_seeds=len(label_seeds), gold_source_seeds=len(gold_seeds),
                          overlap_source_seeds=len(overlap), excluded_complete_rows=len(rows) - len(train_rows) - len(valid_rows),
                          training_rows=len(train_rows), validation_rows=len(valid_rows),
                          training_seeds=len({row["seed"] for row in train_rows}),
                          validation_seeds=len({row["seed"] for row in valid_rows}))), flush=True)

    torch.set_num_threads(1)
    device = torch.device(args.device)
    base = load_policy(args.checkpoint, device)
    policy = MapDistillPolicy(base.config, 1., device).to(device).eval()
    policy.load_state_dict(base.state_dict())
    policy.encoder_fp16 = base.encoder_fp16
    policy.requires_grad_(False)
    del base
    optimizer = torch.optim.Adam(policy.route_head.parameters(), lr=HEAD_LEARNING_RATE)
    rng = random.Random(SPLIT_SEED)
    for epoch in range(1, args.epochs + 1):
        rng.shuffle(train_targets)
        training_kl = 0.
        for start in range(0, len(train_targets), LABELS_PER_UPDATE):
            batch = train_targets[start:start + LABELS_PER_UPDATE]
            inputs, teacher = batch_targets(batch, device)
            step = train_head_step(policy, optimizer, inputs, teacher)
            training_kl += step["loss"] * len(batch)
        metrics = validation_metrics(policy, valid_rows, valid_targets, device)
        print(json.dumps(dict(event="map_offline_epoch", epoch=epoch,
                              training_kl=training_kl / len(train_targets), **metrics), allow_nan=False), flush=True)
    save_snapshot(policy, args.output)


if __name__ == "__main__":
    main()
