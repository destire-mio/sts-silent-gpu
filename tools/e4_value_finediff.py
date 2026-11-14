"""Compare small changes to copied E4 roots with paired terminal rollouts."""
import argparse
import importlib
import json
from pathlib import Path
import random
import statistics
import time

import torch

from tools.e4_value_truth import EXAM_SEEDS, RootCollector, collector, load_policy, mean_se, remaining_returns


NODES = ("enter_act2", "before_act2_boss", "enter_act3")
CHANGES = ("hp_minus_5", "hp_minus_10", "remove_potion", "add_injury")
SKIP_REASONS = ("insufficient_hp", "insufficient_hp", "no_potion", "full_deck")


def pearson(left, right):
    if len(set(left)) > 1 and len(set(right)) > 1:
        return statistics.correlation(left, right)
    return None


def paired_statistics(baseline, changed, rollouts):
    base = {row["seed"]: row for row in baseline}
    pairs = []
    for row in changed:
        control = base[row["seed"]]
        control_trials = {t["seed"]: t["remaining_return"] for t in control["futures"]}
        paired_returns = []
        for trial in row["futures"]:
            control_return = control_trials[trial["seed"]]
            if control_return is not None and trial["remaining_return"] is not None:
                paired_returns.append(trial["remaining_return"] - control_return)
        pairs.append(dict(seed=row["seed"], paired_futures=len(paired_returns),
                          delta_v=row["value"] - control["value"],
                          delta_return=mean_se(paired_returns)[0] if len(paired_returns) == rollouts else None))
    complete = [row for row in pairs if row["paired_futures"] == rollouts]
    delta_v = [row["delta_v"] for row in complete]
    delta_return = [row["delta_return"] for row in complete]
    mean_v, se_v = mean_se(delta_v)
    mean_return, se_return = mean_se(delta_return)
    agreement = [(v > 0, v < 0) == (r > 0, r < 0) for v, r in zip(delta_v, delta_return)]
    return dict(eligible_roots=len(changed), complete_pairs=len(complete),
                paired_futures=sum(row["paired_futures"] for row in pairs),
                delta_v=mean_v, delta_v_se=se_v, delta_return=mean_return, delta_return_se=se_return,
                pearson=pearson(delta_v, delta_return), sign_agreement=mean_se(agreement)[0], pairs=pairs)


@torch.no_grad()
def audit_roots(runner, model, roots, rollouts, seed):
    search = runner.search
    backend = "spire_gpu_finediff" if search.device.type == "cuda" else "spire_finediff"
    perturb_slots = importlib.import_module(backend).perturb_slots
    versions, skipped, draw_seeds = {}, {}, {}
    rng = random.Random(seed)
    used_keys = set()
    for node, group in roots.items():
        for root in group:
            keys = []
            while len(keys) < rollouts:
                draw = rng.randrange(2**63)
                if draw not in used_keys:
                    keys.append(draw)
                    used_keys.add(draw)
            draw_seeds[root["slot"]] = keys
        versions[node, "control"] = group
        for change, (name, reason) in enumerate(zip(CHANGES, SKIP_REASONS)):
            variants = []
            excluded = []
            versions[node, name] = variants
            skipped[node, name] = excluded
            if not group:
                continue
            slots = search.take(len(group))
            search.clone([root["slot"] for root in group], slots)
            applied = perturb_slots(search.batch, slots, change)
            for root, slot, changed in zip(group, slots, applied):
                if not changed:
                    excluded.append(dict(seed=root["seed"], reason=reason))
                    search.available.append(slot)
                    continue
                draw_seeds[slot] = draw_seeds[root["slot"]]
                variants.append(dict(root, slot=slot))
            progress = search.progress_for([root["slot"] for root in variants]) if variants else {}
            for root in variants:
                root["progress"] = progress[root["slot"]]

    observed, _ = remaining_returns(runner, model, versions, rollouts, seed,
                                    draw_seeds=draw_seeds, sample_decisions=False)
    results = {}
    all_controls = []
    for node in roots:
        control = observed[node, "control"]
        all_controls.extend(row for row in control["states"] if row["complete_futures"] == rollouts)
        changes = {}
        for name in CHANGES:
            changed = observed[node, name]["states"]
            changes[name] = dict(paired_statistics(control["states"], changed, rollouts),
                                 skipped=skipped[node, name], variants=changed)
        results[node] = dict(control=control, changes=changes)
    return dict(nodes=results, control_complete_roots=len(all_controls),
                control_pearson=pearson([r["value"] for r in all_controls],
                                        [r["mean_return"] for r in all_controls]))


def report(result):
    def number(n):
        return "NA" if n is None else f"{n:.5f}"

    lines = ["# E4 value fine differences", "",
             "Differences are modified minus control. SEs use one mean per fully paired root.",
             "A sign tie agrees only with another tie.", "",
             f"Pooled control Pearson {number(result['control_pearson'])} "
             f"over {result['control_complete_roots']} complete roots.", "",
             "| Milestone | Variant | Roots | Mean ΔV ± SE | Mean Δreturn ± SE | Pearson | Sign agreement |",
             "| --- | --- | ---: | ---: | ---: | ---: | ---: |"]
    for node, group in result["nodes"].items():
        control = group["control"]
        lines.append(f"| {node} | control | {control['complete_roots']}/{control['roots']} "
                     f"| | | {number(control['pearson'])} | |")
        for name, row in group["changes"].items():
            pair = f"{row['complete_pairs']}/{row['eligible_roots']}"
            delta_v = f"{number(row['delta_v'])} ± {number(row['delta_v_se'])}"
            delta_return = f"{number(row['delta_return'])} ± {number(row['delta_return_se'])}"
            sign = f"{row['sign_agreement']:.2%}" if row["sign_agreement"] is not None else "NA"
            lines.append(f"| {node} | {name} | {pair} | {delta_v} | {delta_return} "
                         f"| {number(row['pearson'])} | {sign} |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="New directory for JSON and Markdown")
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--roots", type=int, default=256, help="Total root target across three milestones")
    parser.add_argument("--rollouts", type=int, default=32, help="Paired futures per root and variant")
    parser.add_argument("--lanes", type=int, default=128)
    parser.add_argument("--seed", type=int, default=101)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.roots <= len(EXAM_SEEDS) * len(NODES) or args.rollouts < 1 or args.lanes < 1:
        parser.error(f"Use 1 to {len(EXAM_SEEDS) * len(NODES)} roots and positive rollout and lane counts")
    torch.set_num_threads(1)
    device = torch.device(args.device)
    if device.type == "cuda":
        torch.cuda.set_device(device if device.index is not None else 0)
    model = load_policy(args.model, device, args.fp32)
    per_node, extra = divmod(args.roots, len(NODES))
    counts = {node: per_node + (i < extra) for i, node in enumerate(NODES)}
    runner = collector(args.lanes, device, args.seed, float("inf"), greedy=True,
                       storage_slots=args.roots * (1 + len(CHANGES)), cls=RootCollector)
    args.output.mkdir()
    started = time.monotonic()
    roots = runner.capture_roots(model, counts, EXAM_SEEDS)
    result = audit_roots(runner, model, roots, args.rollouts, args.seed)
    result.update(model=str(args.model), device=str(device), lanes=args.lanes, seed=args.seed,
                  requested_roots=counts, exam_seed_start=EXAM_SEEDS.start,
                  exam_seed_stop_exclusive=EXAM_SEEDS.stop,
                  rollouts=args.rollouts, policy="argmax", key_last_chance=False,
                  elapsed_seconds=time.monotonic() - started)
    (args.output / "value-finediff.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    (args.output / "value-finediff.md").write_text(report(result), encoding="utf-8")


if __name__ == "__main__":
    main()
