"""Run an isolated argmax exam with paired map lookahead."""
import argparse
import gzip
import json
from pathlib import Path
import time

import torch

from tools.e4_value_truth import EXAM_SEEDS, load_policy, mean_se
from train.e4_lookahead import MapLookaheadSearch
from train.e4_stage12 import NoSLCollector


def references(paths):
    result = {}
    for path in paths:
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "rt", encoding="utf-8") as stream:
            rows = json.load(stream)
        if isinstance(rows, dict):
            rows = rows["episodes"]
        for row in rows:
            seed = row["seed"]
            if seed in result:
                raise ValueError(f"Duplicate reference seed {seed}")
            result[seed] = row
    return result


def compare(rows, reference):
    paired = [(r, reference[r["seed"]]) for r in rows if r["seed"] in reference]
    if not paired:
        raise ValueError("No exam seeds matched the reference")
    metrics = {
        "floor": lambda r: r["floor"] if "floor" in r else r["final_floor"],
        "act2_boss": lambda r: int(r["reach"]["act2_boss"]),
        "act3_first_boss": lambda r: int(r["reach"]["act3_first_boss"]),
    }
    result = {}
    for name, value in metrics.items():
        candidate = [value(a) for a, _ in paired]
        baseline = [value(b) for _, b in paired]
        change, se = mean_se([a - b for a, b in zip(candidate, baseline)])
        result[name] = dict(lookahead=sum(candidate), argmax=sum(baseline),
                            mean_change=change, paired_se=se)
    return dict(seeds=[a["seed"] for a, _ in paired], metrics=result)


def evaluate(policy, seeds, reference, device, lanes, k, *, limit=600):
    started = time.monotonic()
    search = MapLookaheadSearch(lanes, device, 130, k, limit)
    collector = NoSLCollector(search, torch.Generator().manual_seed(130))
    rows = sorted(collector.evaluate(policy, seeds), key=lambda r: r["seed"])
    episodes = [{key: row[key] for key in ("seed", "floor", "reach", "outcome", "stall_class")}
                for row in rows]
    return dict(k=k, cap=limit, episodes=episodes, comparison=compare(episodes, reference),
                preview_decisions=search.preview_decisions, overrides=search.overrides,
                failed_previews=search.failed_previews, preview_steps=search.preview_steps,
                average_steps_per_decision=(search.preview_steps / search.preview_decisions
                                            if search.preview_decisions else None),
                wall_seconds=time.monotonic() - started)


def markdown(result):
    def fmt(number):
        return "—" if number is None else f"{number:.6f}"

    n = len(result["comparison"]["seeds"])
    lines = ["# E4 map lookahead exam", "",
             f"Ran {len(result['episodes'])} seeds. {n} matched the original argmax reference.",
             f"K={result['k']}, horizon cap {result['cap']} steps per future.",
             f"Map decisions {result['preview_decisions']}, changed {result['overrides']}, "
             f"with incomplete futures {result['failed_previews']}.",
             f"Simulated steps {result['preview_steps']} including the initial map choices. "
             f"Mean simulated steps per map decision {fmt(result['average_steps_per_decision'])}. "
             f"Wall seconds {result['wall_seconds']:.3f}.", "",
             "| Metric | Map lookahead | Argmax | Paired change | Paired SE |",
             "| --- | ---: | ---: | ---: | ---: |"]
    for name, row in result["comparison"]["metrics"].items():
        a, b = row["lookahead"], row["argmax"]
        if name == "floor":
            values = f"{a/n:.6f} | {b/n:.6f}"
        else:
            values = f"{a}/{n} ({a/n:.2%}) | {b}/{n} ({b/n:.2%})"
        lines.append(f"| {name} | {values} | {fmt(row['mean_change'])} | {fmt(row['paired_se'])} |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--seeds-count", type=int, default=256)
    parser.add_argument("--k", type=int, default=16)
    parser.add_argument("--reference", nargs="+", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=128)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.seeds_count <= len(EXAM_SEEDS) or args.k < 2 or args.lanes < 1:
        parser.error("Choose 1 to 1024 seeds, K of at least 2, and at least one lane")
    seeds = list(EXAM_SEEDS[:args.seeds_count])
    reference = references(args.reference)
    if not set(seeds).intersection(reference):
        parser.error("The reference has none of the selected exam seeds")
    torch.set_num_threads(1)
    device = torch.device(args.device)
    model = load_policy(args.checkpoint, device, args.fp32)
    args.output.mkdir()
    result = evaluate(model, seeds, reference, device, args.lanes, args.k)
    (args.output / "map-exam.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    (args.output / "map-exam.md").write_text(markdown(result), encoding="utf-8")


if __name__ == "__main__":
    main()
