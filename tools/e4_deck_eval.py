"""Collect matched Boss outcomes, fit a deck win model, and test it on card choices."""
import argparse
from collections import Counter, defaultdict
import gzip
import importlib
from itertools import combinations
import json
import math
from pathlib import Path
import random
import statistics
import time

import torch
from torch import nn
from torch.nn.functional import binary_cross_entropy_with_logits

from tools.e4_boss_probe import run_trials, seed_range
from tools.e4_value_truth import RootCollector, load_policy
from train.e4 import ACT, FLOOR, SCREEN
from train.e4_stage12 import NoSLCollector, NoSLSearch
from train.model import SCREENS


NODES = ("before_act1_boss", "before_act2_boss", "before_act3_boss", "before_act3_second_boss")
FORMAT = "e4_deck_eval_v1"
FEATURES = ("cards", "relics", "potions", "hp", "max_hp", "act", "boss")


def backend(device):
    return importlib.import_module("spire_gpu_finediff" if device.type == "cuda" else "spire_finediff")


def deck_edit(root, pool, capacity, rng):
    can_remove = min(2, len(root["removable"]), len(root["cards"]) - 1)
    can_add = min(2, capacity - len(root["cards"]))
    directions = [direction for direction, capacity in ((-1, can_remove), (1, can_add)) if capacity > 0]
    if not directions:
        return None
    if rng.choice(directions) < 0:
        positions = sorted(rng.sample(root["removable"], rng.randint(1, can_remove)), reverse=True)
        return (positions + [-1, -1])[:2] + [0, 0]
    cards = rng.choices(pool, k=rng.randint(1, can_add))
    return [-1, -1] + (cards + [0, 0])[:2]


def label_roots(runner, model, grouped, variants, draws, rng):
    search = runner.search
    native = backend(search.device)
    ordered = [(node, root) for node, roots in grouped.items() for root in roots]
    if not ordered:
        return []
    features = native.deck_features(search.batch, [root["slot"] for _, root in ordered])
    candidates, capacity = native.silent_reward_card_ids(), native.max_deck_cards()
    conditions, pending = [], []
    for (node, root), feature in zip(ordered, features):
        if node == NODES[-1] and not feature["second_boss_available"]:
            continue
        seeds = rng.sample(range(2**63 - 1), draws)
        conditions.append((node, root, 0, seeds))
        for variant in range(1, variants + 1):
            edit = deck_edit(feature, candidates, capacity, rng)
            if edit is not None:
                pending.append((node, root, variant, seeds, edit))

    copies = search.take(len(pending)) if pending else []
    try:
        if copies:
            search.clone([root["slot"] for _, root, _, _, _ in pending], copies)
            native.edit_decks(search.batch, copies, [edit for _, _, _, _, edit in pending])
        for slot, (node, root, variant, seeds, _) in zip(copies, pending):
            conditions.append((node, dict(root, slot=slot), variant, seeds))
        features = native.deck_features(search.batch, [root["slot"] for _, root, _, _ in conditions])
        plan = [(root, seed) for _, root, _, seeds in conditions for seed in seeds]
        outcomes = run_trials(runner, model, plan, enter_boss=True)
        rows = []
        for (node, root, variant, seeds), feature in zip(conditions, features):
            by_seed = {trial["seed"]: trial for trial in outcomes[root["slot"]]}
            trials = [by_seed[seed] for seed in seeds]
            completed = sum(row["outcome"] != "incomplete" for row in trials)
            wins = sum(row["outcome"] == "win" for row in trials)
            rows.append(dict(seed=root["seed"], node=node, variant=variant, draw_seeds=seeds,
                split="holdout" if root["seed"] % 10 == 0 else "train",
                features={key: feature[key] for key in FEATURES}, trials=trials,
                wins=wins, completed=completed, total_draws=draws,
                win_rate=wins / draws if completed == draws else None,
                remaining_hp_ratio=statistics.mean(row["remaining_hp_ratio"] for row in trials)
                    if completed == draws else None))
        return rows
    finally:
        search.available.extend(copies)


def collection_rows(model, device, seeds, roots_per_node, lanes, variants, draws, random_seed):
    lanes = min(lanes, len(seeds))
    search = NoSLSearch(lanes, device, random_seed, storage_slots=len(NODES) * lanes * (variants + 1), greedy=True)
    runner = RootCollector(search, torch.Generator().manual_seed(random_seed))
    rng = random.Random(random_seed)
    counts = Counter()
    for offset in range(0, len(seeds), lanes):
        batch = seeds[offset:offset + lanes]
        quotas = {node: min(len(batch), roots_per_node - counts[node]) for node in NODES}
        if not any(quotas.values()):
            return
        runner.seeds[:] = [None] * lanes
        grouped = runner.capture_roots(model, quotas, batch)
        try:
            rows = label_roots(runner, model, grouped, variants, draws, rng)
            counts.update(row["node"] for row in rows if row["variant"] == 0)
            yield from rows
        finally:
            search.available.extend(root["slot"] for group in grouped.values() for root in group)


def write_collection(rows, output, vocab_sizes, *, variants, draws):
    output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    shard = output / "labels.jsonl.gz"
    counts, splits = Counter(), Counter()
    total = complete = 0
    with gzip.open(shard, "wt", encoding="utf-8") as stream:
        for row in rows:
            stream.write(json.dumps(row, allow_nan=False, separators=(",", ":")) + "\n")
            total += 1
            complete += row["completed"] == row["total_draws"]
            if row["variant"] == 0:
                counts[row["node"]] += 1
                splits[row["split"]] += 1
    seconds, size, count = time.perf_counter() - started, shard.stat().st_size, sum(counts.values())
    result = dict(format=FORMAT, vocab_sizes=list(vocab_sizes), variants=variants, draws=draws,
        roots=count, roots_by_node={node: counts[node] for node in NODES},
        node_status={node: "available" if counts[node] else "unavailable" for node in NODES},
        roots_by_split=dict(splits), rows=total, complete_rows=complete, seconds=seconds, shard=shard.name,
        bytes=size, seconds_per_1000_roots=1000 * seconds / count if count else None,
        bytes_per_1000_roots=1000 * size / count if count else None)
    write_json(output / "manifest.json", result)
    return result


def packed(rows, device):
    width = {key: max(1, *(len(row[key]) for row in rows)) for key in ("cards", "relics", "potions")}
    cards = torch.zeros((len(rows), width["cards"], 2), dtype=torch.long)
    relics = torch.zeros((len(rows), width["relics"]), dtype=torch.long)
    potions = torch.zeros((len(rows), width["potions"]), dtype=torch.long)
    for i, row in enumerate(rows):
        if row["cards"]:
            cards[i, :len(row["cards"])] = torch.as_tensor(row["cards"])
        if row["relics"]:
            relics[i, :len(row["relics"])] = torch.as_tensor(row["relics"])
        if row["potions"]:
            potions[i, :len(row["potions"])] = torch.as_tensor(row["potions"])
    boss = torch.tensor([row["boss"] for row in rows], dtype=torch.long)
    scalars = torch.tensor([[row["hp"] / row["max_hp"], row["act"] / 3., len(row["cards"]) / 40.]
                             for row in rows], dtype=torch.float32)
    return tuple(value.to(device) for value in (cards, relics, potions, boss, scalars))


class DeckEvaluator(nn.Module):
    def __init__(self, vocab_sizes):
        super().__init__()
        cards, relics, potions, bosses = vocab_sizes
        self.cards = nn.Embedding(cards, 32, padding_idx=0)
        self.card_features = nn.Sequential(nn.Linear(33, 32), nn.GELU())
        self.relics = nn.Embedding(relics, 16, padding_idx=0)
        self.potions = nn.Embedding(potions, 8, padding_idx=0)
        self.boss = nn.Embedding(bosses, 16)
        self.score = nn.Sequential(nn.Linear(75, 64), nn.GELU(), nn.Linear(64, 1))

    def forward(self, cards, relics, potions, boss, scalars):
        ids, upgrades = cards.unbind(dim=-1)
        mask = (ids != 0).unsqueeze(-1)
        deck = self.card_features(torch.cat((self.cards(ids), upgrades.float().unsqueeze(-1)), dim=-1))
        deck = (deck * mask).sum(1) / mask.sum(1).clamp_min(1).sqrt()
        return self.score(torch.cat((deck, self.relics(relics).sum(1), self.potions(potions).sum(1),
                                     self.boss(boss), scalars), dim=1)).squeeze(-1)


def holdout_metrics(rows, probabilities):
    pairs = defaultdict(list)
    ranked = defaultdict(lambda: [0, 0])
    wins = losses = 0
    squared_error = 0.
    for row, probability in zip(rows, probabilities):
        won = row["wins"]
        lost = row["total_draws"] - won
        wins += won
        losses += lost
        squared_error += won * (1 - probability) ** 2 + lost * probability ** 2
        ranked[probability][0] += won
        ranked[probability][1] += lost
        pairs[row["seed"], row["node"]].append((row["win_rate"], probability))
    cumulative_losses = credit = 0.
    for _, (won, lost) in sorted(ranked.items()):
        credit += won * (cumulative_losses + lost / 2)
        cumulative_losses += lost
    matches = 0.
    comparisons = 0
    for group in pairs.values():
        for (target, score), (other_target, other_score) in combinations(group, 2):
            if target == other_target:
                continue
            comparisons += 1
            matches += .5 if score == other_score else (target > other_target) == (score > other_score)
    return dict(rows=len(rows), draws=wins + losses,
        brier=squared_error / (wins + losses) if wins + losses else None,
        auc=credit / (wins * losses) if wins and losses else None,
        same_root_order_accuracy=matches / comparisons if comparisons else None,
        same_root_comparisons=comparisons)


def fit(data, output, device, steps, batch_size, lr, seed):
    manifest = json.loads((data / "manifest.json").read_text(encoding="utf-8"))
    if manifest["format"] != FORMAT:
        raise ValueError("Unrecognized deck label format")
    train, holdout = [], []
    with gzip.open(data / manifest["shard"], "rt", encoding="utf-8") as stream:
        for line in stream:
            row = json.loads(line)
            if row["win_rate"] is not None:
                (holdout if row["seed"] % 10 == 0 else train).append(row)
    if not train:
        raise ValueError("The collection has no complete training labels")
    torch.manual_seed(seed)
    rng = random.Random(seed)
    model = DeckEvaluator(manifest["vocab_sizes"]).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=lr)
    for _ in range(steps):
        rows = rng.sample(train, min(batch_size, len(train)))
        predictions = model(*packed([row["features"] for row in rows], device))
        targets = torch.tensor([row["win_rate"] for row in rows], device=device)
        counts = torch.tensor([row["total_draws"] for row in rows], device=device)
        loss = (binary_cross_entropy_with_logits(predictions, targets, reduction="none") * counts).sum() / counts.sum()
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
    probabilities = []
    model.eval()
    with torch.no_grad():
        for offset in range(0, len(holdout), batch_size):
            rows = holdout[offset:offset + batch_size]
            probabilities.extend(model(*packed([row["features"] for row in rows], device)).sigmoid().tolist())
    output.mkdir(parents=True, exist_ok=False)
    result = dict(training_rows=len(train), steps=steps, last_loss=float(loss.detach()),
                  holdout=holdout_metrics(holdout, probabilities))
    torch.save(dict(format=FORMAT, vocab_sizes=manifest["vocab_sizes"], model=model.state_dict()), output / "deck_eval.pt")
    write_json(output / "fit.json", result)
    return result


class DeckChoiceSearch(NoSLSearch):
    def __init__(self, lanes, device, evaluator, *, key_last_chance=False):
        super().__init__(lanes, device, 130, storage_slots=9 * lanes, greedy=True, key_last_chance=key_last_chance)
        self.evaluator = evaluator
        self.native = backend(device)
        self.stats = {kind: dict(preview_decisions=0, overrides=0, preview_steps=0, failed_previews=0)
                      for kind in ("combat", "card", "map")}
        self.preview_faults = []

    @torch.no_grad()
    def predict(self, model, slots):
        actions, values, logs = super().predict(model, slots)
        active = [(i, slot) for i, slot in enumerate(slots) if slot not in self.failed
                  and self.mainline_progress[slot][ACT] <= 3
                  and SCREENS[self.mainline_progress[slot][SCREEN]] == "card_reward"]
        if not active:
            return actions, values, logs
        positions, parents = zip(*active)
        kinds = self.arrays["candidate_kinds"][list(parents)]
        mask = self.last_policy_mask[list(positions), :kinds.shape[1]] & ((kinds == 3) | (kinds == 7))
        selected = torch.tensor([actions[i] for i in positions], device=self.device)
        eligible = (mask.sum(1) > 1) & mask.gather(1, selected[:, None]).squeeze(1)
        choices = [(positions[i], parents[i], action) for i, action in torch.nonzero(mask & eligible[:, None]).tolist()]
        if not choices:
            return actions, values, logs

        stats = self.stats["card"]
        original = {slot: row for slot, row in zip(parents, self.native.deck_features(self.batch, list(parents)))}
        scratch = self.take(len(choices))
        try:
            self.batch.clone_many([slot for _, slot, _ in choices], scratch)
            self.batch.step_slots(scratch, [action for _, _, action in choices])
            stats["preview_steps"] += len(scratch)
            faults = self.arrays["faults"].to(torch.int64)[scratch].tolist()
            good_slots, good_choices = [], []
            for target, fault, choice in zip(scratch, faults, choices):
                if fault:
                    stats["failed_previews"] += 1
                    if len(self.preview_faults) < 20:
                        _, slot, _ = choice
                        self.preview_faults.append(dict(seed=self.provenance[slot][0],
                            floor=self.mainline_progress[slot][FLOOR], kind="card", fault=fault & 0xFFFFFFFF))
                else:
                    good_slots.append(target)
                    good_choices.append(choice)
            if good_slots:
                changed = self.native.deck_features(self.batch, good_slots)
                rows = [dict(original[slot], cards=row["cards"]) for (_, slot, _), row in zip(good_choices, changed)]
                scores = self.evaluator(*packed(rows, self.device)).tolist()
                best = {}
                for (i, _, action), score in zip(good_choices, scores):
                    if i not in best or score > best[i][0] or (score == best[i][0] and action == actions[i]):
                        best[i] = score, action
                stats["preview_decisions"] += len(best)
                for i, (_, action) in best.items():
                    stats["overrides"] += action != actions[i]
                    actions[i] = action
        finally:
            self.available.extend(scratch)
        return actions, values, logs


def choose(model, evaluator, device, seeds, lanes, *, key_last_chance=False):
    started = time.monotonic()
    search = DeckChoiceSearch(min(lanes, len(seeds)), device, evaluator, key_last_chance=key_last_chance)
    runner = NoSLCollector(search, torch.Generator().manual_seed(130))
    episodes = runner.evaluate(model, seeds)
    return dict(episodes=episodes, seconds=time.monotonic() - started,
        mainline_decisions=runner.decisions, search=search.stats, preview_faults=search.preview_faults,
        combat_draws=0, combat_options=0, combat_scope="all", override_se=None, validate=0,
        cards="on", card_policy="deck_eval", map_k=0, key_last_chance=key_last_chance)


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    collect_parser = sub.add_parser("collect")
    collect_parser.add_argument("--seeds", type=seed_range, default=range(1_900_000_000, 1_900_100_000))
    collect_parser.add_argument("--roots", type=int, default=256, help="Maximum roots per Boss node")
    collect_parser.add_argument("--variants", type=int, default=3)
    collect_parser.add_argument("--draws", type=int, default=16)
    fit_parser = sub.add_parser("fit")
    fit_parser.add_argument("--data", type=Path, required=True)
    fit_parser.add_argument("--steps", type=int, default=5000)
    fit_parser.add_argument("--batch-size", type=int, default=128)
    fit_parser.add_argument("--lr", type=float, default=3e-4)
    choose_parser = sub.add_parser("choose")
    choose_parser.add_argument("--deck-eval", type=Path, required=True)
    choose_parser.add_argument("--seed-start", type=int, default=2_000_000_000)
    choose_parser.add_argument("--episodes", type=int, default=128)
    choose_parser.add_argument("--key-last-chance", action="store_true")
    for cmd in (collect_parser, fit_parser, choose_parser):
        cmd.add_argument("--device", default="cpu")
        cmd.add_argument("--output", type=Path, required=True)
    for cmd in (collect_parser, choose_parser):
        cmd.add_argument("--model", type=Path, required=True)
        cmd.add_argument("--lanes", type=int, default=128)
    for cmd in (collect_parser, fit_parser):
        cmd.add_argument("--random-seed", type=int, default=130)
    args = parser.parse_args(argv)
    torch.set_num_threads(1)
    device = torch.device(args.device)
    if args.command == "collect":
        if (min(args.roots, args.lanes, args.draws) < 1 or args.variants < 0 or not args.seeds
                or args.seeds.start < 1_000_000_000 or args.seeds.stop > 2_000_000_000):
            parser.error("Use nonempty training seeds within 1000000000:2000000000 and a positive collection size")
        rows = collection_rows(load_policy(args.model, device), device, args.seeds,
                               args.roots, args.lanes, args.variants, args.draws, args.random_seed)
        print(json.dumps(write_collection(rows, args.output, backend(device).deck_vocab_sizes(),
                                          variants=args.variants, draws=args.draws), allow_nan=False))
    elif args.command == "fit":
        if min(args.steps, args.batch_size) < 1 or not math.isfinite(args.lr) or args.lr <= 0:
            parser.error("Use positive steps, batch size and learning rate")
        print(json.dumps(fit(args.data, args.output, device, args.steps, args.batch_size,
                             args.lr, args.random_seed), allow_nan=False))
    else:
        if min(args.lanes, args.episodes) < 1:
            parser.error("Use a positive lane and episode count")
        saved = torch.load(args.deck_eval, map_location=device, weights_only=True)
        if saved["format"] != FORMAT or saved["vocab_sizes"] != list(backend(device).deck_vocab_sizes()):
            parser.error("The evaluator format or native vocabulary differs from the checkpoint")
        evaluator = DeckEvaluator(saved["vocab_sizes"]).to(device).eval()
        evaluator.load_state_dict(saved["model"])
        seeds = range(args.seed_start, args.seed_start + args.episodes)
        result = choose(load_policy(args.model, device), evaluator, device, seeds, args.lanes,
                        key_last_chance=args.key_last_chance)
        write_json(args.output, result)


if __name__ == "__main__":
    main()
