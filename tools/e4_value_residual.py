"""Calibrate E4 value residuals on ordinary sampled-policy training episodes."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import time

import torch

from tools.e4_value_truth import RootCollector, collector, load_policy, mean_se
from train.e4 import ACT, FLOOR, MAP, MAX_REWARD, SCREEN

NODES = ("after_act1_boss", "enter_act2", "before_act2_boss", "enter_act3", "before_act3_boss")
CARD_TYPES = ("Attack", "Skill", "Power")


def training_seeds(specs):
    """Accept individual seeds and half-open start:stop ranges."""
    seeds = []
    for spec in specs:
        first, separator, last = spec.partition(":")
        start = int(first)
        stop = int(last) if separator else start + 1
        if not 1_000_000_000 <= start < stop <= 2_000_000_000:
            raise ValueError("Training seeds must lie in [1000000000, 2000000000)")
        seeds.extend(range(start, stop))
    if len(set(seeds)) != len(seeds):
        raise ValueError("Training seeds must be unique")
    return seeds


def card_catalog():
    root = Path(__file__).resolve().parents[1]
    entries = re.findall(r'SPIRE_UNIT\([^,]+, "([^"]+)", "([^"]+)"',
                         (root / "core/cards/catalog.def").read_text(encoding="utf-8"))
    catalog = {}
    for card_id, (name, source) in enumerate(entries, 1):
        card = (root / source).read_text(encoding="utf-8")
        match = re.search(r'(?:CardDef\s+\w+\s*|return\s*)\{\s*CardType::(\w+)', card)
        if match is None:
            raise ValueError(f"Missing native type for {source}")
        catalog[card_id] = {"name": name, "type": match[1]}
    return catalog


def deck_snapshot(search, slot, catalog):
    tags = search.arrays["tags"][slot, :2]
    master = (tags[0] == 3) & (tags[1] == 1)
    deck = Counter(search.arrays["ids"][slot, 0, master].long().tolist())
    counts = Counter()
    for card, count in deck.items():
        counts[catalog[card]["type"]] += count
    return dict(cards=sorted(deck), type_counts={kind: counts[kind] for kind in CARD_TYPES})


@torch.no_grad()
def measure(model, runner, seeds, catalog):
    search = runner.search
    waiting = iter(seeds)
    pending = [{} for _ in range(search.lanes)]
    completed = {}
    while True:
        search.clock()
        free = [slot for slot, seed in enumerate(runner.seeds) if seed is None]
        batch = [seed for _, seed in zip(free, waiting)]
        if batch:
            fresh = free[:len(batch)]
            runner.reset(fresh, batch)
            for slot in fresh:
                pending[slot].clear()
        live = [slot for slot, seed in enumerate(runner.seeds) if seed is not None]
        if not live:
            break
        progress = search.progress_for(live)
        actions, values, _, _ = runner.choose(model, live, progress)
        boss_ready = runner.boss_choices([
            slot for slot in live if slot not in search.failed
            and progress[slot][SCREEN] == MAP and progress[slot][ACT] in (2, 3)
        ])
        for slot, value in zip(live, values):
            if slot in search.failed:
                continue
            act = progress[slot][ACT]
            ledger = runner.rewards[slot]
            nodes = []
            if act == 1 and ledger.reach["act1_boss"]:
                nodes.append("after_act1_boss")
            if act in (2, 3):
                nodes.append(f"enter_act{act}")
            if slot in boss_ready and not ledger.reach["act3_first_boss"]:
                nodes.append(f"before_act{act}_boss")
            for node in nodes:
                if node not in pending[slot]:
                    pending[slot][node] = dict(floor=progress[slot][FLOOR], value=value * MAX_REWARD,
                                               earned=ledger.total, **deck_snapshot(search, slot, catalog))
        _, terminal = runner.advance(live, actions, progress)
        for slot, episode in zip((s for s, done in zip(live, terminal) if done), runner.completed):
            complete = episode["outcome"] != "stalled"
            for sample in pending[slot].values():
                actual = runner.rewards[slot].total - sample["earned"] if complete else None
                sample.update(actual=actual, residual=actual - sample["value"] if complete else None)
            completed[episode["seed"]] = dict(seed=episode["seed"], outcome=episode["outcome"],
                                               stall_class=episode["stall_class"],
                                               milestones=dict(pending[slot]))
        runner.completed.clear()
    return [completed[seed] for seed in seeds]


def residual_stats(rows):
    mean, se = mean_se([row["residual"] for row in rows])
    return dict(samples=len(rows), mean=mean, se=se)


def summarize(episodes):
    groups = {}
    for node in NODES:
        observed = [ep["milestones"][node] for ep in episodes if node in ep["milestones"]]
        rows = [row for row in observed if row["residual"] is not None]
        present = Counter(card for row in rows for card in row["cards"])
        cards = {}
        for card, count in sorted(present.items()):
            if count >= 30:
                cards[card] = dict(present=residual_stats([row for row in rows if card in row["cards"]]),
                                   absent=residual_stats([row for row in rows if card not in row["cards"]]))
        powers = {}
        for count in (0, 1, 2, 3):
            label = str(count) if count < 3 else "3+"
            powers[label] = residual_stats([row for row in rows if min(row["type_counts"]["Power"], 3) == count])
        groups[node] = dict(observed=len(observed), all=residual_stats(rows), cards=cards, powers=powers)
    return groups


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--seeds", nargs="+", required=True, help="Training seed IDs or half-open start:stop ranges")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=512)
    parser.add_argument("--policy-seed", type=int, default=102)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    try:
        seeds = training_seeds(args.seeds)
    except ValueError as error:
        parser.error(str(error))
    if args.lanes < 1:
        parser.error("--lanes must be positive")
    torch.set_num_threads(1)
    device = torch.device(args.device)
    catalog = card_catalog()
    args.output.mkdir()
    model = load_policy(args.checkpoint, device, args.fp32)
    # The value-truth continuation samples from the masked policy using seed + 1.
    runner = collector(min(args.lanes, len(seeds)), device, args.policy_seed, float("inf"), cls=RootCollector)
    started = time.monotonic()
    episodes = measure(model, runner, seeds, catalog)
    payload = dict(checkpoint=str(args.checkpoint), policy_seed=args.policy_seed, lanes=runner.search.lanes,
                   device=str(device), fp32=args.fp32, policy="sampled_no_sl",
                   units="undiscounted_remaining_current_reward", value_scale=MAX_REWARD,
                   seconds=time.monotonic() - started, catalog=catalog, groups=summarize(episodes),
                   episodes=episodes)
    (args.output / "value-residual.json").write_text(
        json.dumps(payload, indent=2, allow_nan=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
