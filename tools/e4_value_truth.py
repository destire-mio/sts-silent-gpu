"""Frozen no-SL exams and remaining-return checks for the E4 384 policy."""
import argparse
from copy import deepcopy
import json
import math
from pathlib import Path
import random
import statistics
import time

import torch

from train.e4 import ACT, MAP, MAX_REWARD, SCREEN
from train.e4_stage12 import CLSPolicy, DECISION_TYPES, NoSLCollector, NoSLSearch, decision_type
from train.model import ModelConfig, network_inputs


EXAM_SEEDS = range(2_000_000_000, 2_000_001_024)
NODES = ("before_act1_boss", "enter_act2", "before_act2_boss", "enter_act3")


def load_policy(path, device, fp32=False):
    weights = torch.load(path, map_location="cpu", weights_only=True)
    config = ModelConfig(width=384, layers=6, heads=6, feedforward=1536)
    if weights.get("format") == "e4_map_distill_policy_v1":
        from train.e4_map_distill import MapDistillPolicy
        model = MapDistillPolicy(config, weights["map_weight"], device)
        model.route_head.load_state_dict(weights["map_head"])
        weights = weights["model"]
    else:
        model = CLSPolicy(config)
    model.load_state_dict(weights)
    model.encoder_fp16 = device.type == "cuda" and not fp32
    return model.to(device).eval()


def collector(lanes, device, seed, deadline, *, greedy=False, storage_slots=0, cls=NoSLCollector):
    search = NoSLSearch(lanes, device, seed, storage_slots=storage_slots, greedy=greedy)
    search.deadline = deadline
    return cls(search, torch.Generator().manual_seed(seed))


def mean_se(values):
    return (statistics.mean(values) if values else None,
            statistics.stdev(values) / math.sqrt(len(values)) if len(values) > 1 else None)


def exam_comparison(sample, argmax):
    def by_seed(rows):
        indexed = {row["seed"]: row for row in rows}
        assert len(indexed) == len(rows)
        return indexed

    sample, argmax = by_seed(sample), by_seed(argmax)
    assert sample.keys() == argmax.keys()
    paired = []
    for seed in sorted(sample):
        a, b = sample[seed], argmax[seed]
        paired.append(dict(seed=seed, sampled_floor=a["floor"], argmax_floor=b["floor"],
                           sampled_act2=a["reach"]["act2_boss"], argmax_act2=b["reach"]["act2_boss"],
                           sampled_outcome=a["outcome"], argmax_outcome=b["outcome"]))
    floor, floor_se = mean_se([r["sampled_floor"] - r["argmax_floor"] for r in paired])
    boss, boss_se = mean_se([int(r["sampled_act2"]) - int(r["argmax_act2"]) for r in paired])
    result = dict(episodes=len(paired), paired_floor_change=floor, paired_floor_se=floor_se,
                  paired_act2_change=boss, paired_act2_se=boss_se, seeds=paired)
    for mode in ("sampled", "argmax"):
        cleared = sum(r[f"{mode}_act2"] for r in paired)
        result[mode] = dict(mean_floor=statistics.mean(r[f"{mode}_floor"] for r in paired),
                            act2_cleared=cleared, act2_rate=cleared / len(paired),
                            stalled=sum(r[f"{mode}_outcome"] == "stalled" for r in paired))
    return result


class RootCollector(NoSLCollector):
    def capture(self, model, per_node, max_seeds):
        roots = self.capture_roots(model, dict.fromkeys(NODES, per_node),
                                  range(2_100_000_000, 2_100_000_000 + max_seeds))
        assert all(len(group) == per_node for group in roots.values()), (
            f"Root seed limit reached: { {node: len(group) for node, group in roots.items()} }"
        )
        return roots

    def capture_roots(self, model, counts, seeds):
        roots = {node: [] for node in counts}
        seen = {node: set() for node in counts}
        waiting = iter(seeds)
        while any(len(group) < counts[node] for node, group in roots.items()):
            self.search.clock()
            free = [slot for slot, seed in enumerate(self.seeds) if seed is None]
            seeds = [seed for _, seed in zip(free, waiting)]
            if seeds:
                self.reset(free[:len(seeds)], seeds)
            slots = [slot for slot, seed in enumerate(self.seeds) if seed is not None]
            if not slots:
                break
            progress = self.search.progress_for(slots)
            actions, _, _, _ = self.choose(model, slots, progress)
            maps = [slot for slot in slots if slot not in self.search.failed and
                    progress[slot][SCREEN] == MAP and progress[slot][ACT] in (1, 2)]
            boss_ready = self.boss_choices(maps)
            for slot in slots:
                if slot in self.search.failed:
                    continue
                act = progress[slot][ACT]
                nodes = []
                if slot in boss_ready:
                    nodes.append(f"before_act{act}_boss")
                if act in (2, 3):
                    nodes.append(f"enter_act{act}")
                for node in nodes:
                    seed = self.seeds[slot]
                    if node not in roots or len(roots[node]) == counts[node] or seed in seen[node]:
                        continue
                    target, = self.search.take(1)
                    self.search.clone([slot], [target])
                    roots[node].append(dict(slot=target, seed=seed, progress=progress[slot],
                                            ledger=deepcopy(self.rewards[slot])))
                    seen[node].add(seed)
            self.advance(slots, actions, progress)
            self.completed.clear()
        return roots

    def boss_choices(self, slots):
        if not slots:
            return set()
        arrays = self.search.arrays
        refs = arrays["candidate_refs"][slots, 2].long()
        targets = (refs - 1).clamp_min(0)
        # The observation uses a Control::Boss option as the map's boss destination.
        boss = ((refs > 0) & (arrays["tags"][slots, 0].long().gather(1, targets) == 11)
                & (arrays["ids"][slots, 0].long().gather(1, targets) == 9)).any(1)
        return {slot for slot, present in zip(slots, boss.tolist()) if present}


@torch.no_grad()
def remaining_returns(runner, model, roots, futures, seed, *, draw_seeds=None, sample_decisions=True):
    search = runner.search
    device = search.device
    ordered = [(node, root) for node, group in roots.items() for root in group]
    initial = {}
    for offset in range(0, len(ordered), search.lanes):
        slots = [r["slot"] for _, r in ordered[offset:offset + search.lanes]]
        inputs = network_inputs(search.arrays, device, torch.tensor(slots, device=device))
        initial.update(zip(slots, model(inputs)[1].tolist()))
    by_root = {r["slot"]: [] for _, r in ordered}
    samples = {name: [] for name in DECISION_TYPES}
    randomize = random.Random(seed)
    if draw_seeds is None:
        draw_seeds = {r["slot"]: [randomize.randrange(2**63) for _ in range(futures)] for _, r in ordered}
    planned = [(r, key) for _, r in ordered for key in draw_seeds[r["slot"]]]
    for offset in range(0, len(planned), search.lanes):
        chunk = planned[offset:offset + search.lanes]
        slots = list(range(len(chunk)))
        runner.reset(slots, [r["seed"] for r, _ in chunk])
        search.clone([r["slot"] for r, _ in chunk], slots)
        search.batch.determinize_slots(slots, [key for _, key in chunk], True)
        for slot, key in zip(slots, search.loop_keys(slots)):
            search.seen[slot].add(key)
            search.mask_cache.pop(slot, None)
            search.repeat_blocked.pop(slot, None)
        for slot, (root, _) in enumerate(chunk):
            runner.rewards[slot] = deepcopy(root["ledger"])
        runner.completed.clear()
        counts = [dict.fromkeys(DECISION_TYPES, 0) for _ in slots]
        picks = [{} for _ in slots]
        live = slots
        while live:
            search.clock()
            before = search.progress_for(live)
            actions, values, _, _ = runner.choose(model, live, before)
            if sample_decisions:
                for slot, value in zip(live, values):
                    if slot not in search.failed:
                        kind = DECISION_TYPES[decision_type(before[slot])]
                        counts[slot][kind] += 1
                        if randomize.randrange(counts[slot][kind]) == 0:
                            picks[slot][kind] = (value, runner.rewards[slot].total)
            count = len(runner.completed)
            _, terminal = runner.advance(live, actions, before)
            for slot, episode in zip((s for s, done in zip(live, terminal) if done),
                                     runner.completed[count:]):
                root, draw = chunk[slot]
                total = runner.rewards[slot].total
                complete = episode["outcome"] != "stalled"
                by_root[root["slot"]].append(dict(seed=draw, outcome=episode["outcome"],
                    decisions=len(episode["candidates"]),
                    remaining_return=(total - root["ledger"].total) / MAX_REWARD if complete else None))
                if complete:
                    for kind, (value, earned) in picks[slot].items():
                        samples[kind].append(dict(root_seed=root["seed"], future_seed=draw,
                                                  value=value, remaining_return=(total - earned) / MAX_REWARD))
            live = [s for s, done in zip(live, terminal) if not done]
    result = {}
    for node, group in roots.items():
        rows = []
        for root in group:
            trials = by_root[root["slot"]]
            complete = [t["remaining_return"] for t in trials if t["remaining_return"] is not None]
            actual, se = mean_se(complete)
            rows.append(dict(seed=root["seed"], progress=root["progress"], value=initial[root["slot"]],
                             futures=trials, complete_futures=len(complete), mean_return=actual, future_se=se))
        valid = [row for row in rows if row["complete_futures"] == futures]
        values = [row["value"] for row in valid]
        actuals = [row["mean_return"] for row in valid]
        actual, actual_se = mean_se(actuals)
        bias, bias_se = mean_se([value - actual for value, actual in zip(values, actuals)])
        result[node] = dict(roots=len(rows), complete_roots=len(valid),
                            complete_futures=sum(row["complete_futures"] for row in rows),
                            mean_value=mean_se(values)[0], mean_return=actual, return_se=actual_se,
                            bias=bias, bias_se=bias_se,
                            pearson=statistics.correlation(values, actuals)
                            if len(set(values)) > 1 and len(set(actuals)) > 1 else None,
                            states=rows)
    return result, {kind: dict(samples=len(rows),
        mean_value=mean_se([r["value"] for r in rows])[0],
        mean_return=mean_se([r["remaining_return"] for r in rows])[0],
        bias=mean_se([r["value"] - r["remaining_return"] for r in rows])[0],
        rows=rows) for kind, rows in samples.items()}


def summary(results):
    lines = ["# E4 frozen value check", "",
             "Bias is V minus undiscounted remaining current reward / 156.", ""]
    number = lambda n: "NA" if n is None else f"{n:.5f}"
    for entry in results:
        exam = entry["exam"]
        lines += [f"## {Path(entry['checkpoint']).name}", "",
                  "| Exam | Mean floor | Act 2 boss | Stalled |",
                  "| --- | ---: | ---: | ---: |"]
        for mode in ("argmax", "sampled"):
            row = exam[mode]
            lines.append(f"| {mode} | {number(row['mean_floor'])} | {row['act2_cleared']}/{exam['episodes']} ({row['act2_rate']:.2%}) | {row['stalled']} |")
        lines += ["", f"Sampled minus argmax floor {number(exam['paired_floor_change'])} (paired SE {number(exam['paired_floor_se'])}). "
                  f"Act 2 rate change {number(exam['paired_act2_change'])} (paired SE {number(exam['paired_act2_se'])}).", "",
                  "| Node | Complete roots | Complete futures | V | MC mean | MC SE | Bias | Pearson |",
                  "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
        for node, row in entry["nodes"].items():
            stats = " | ".join(number(row[k]) for k in ("mean_value", "mean_return", "return_se", "bias", "pearson"))
            lines.append(f"| {node} | {row['complete_roots']}/{row['roots']} | {row['complete_futures']} | {stats} |")
        lines += ["", "| Decision | Samples | V | MC mean | Bias |", "| --- | ---: | ---: | ---: | ---: |"]
        for kind, row in entry["decisions"].items():
            stats = " | ".join(number(row[k]) for k in ("mean_value", "mean_return", "bias"))
            lines.append(f"| {kind} | {row['samples']} | {stats} |")
        lines.append("")
    return "\n".join(lines)


@torch.no_grad()
def diagnose(paths, baseline, device, lanes, seed, deadline, *, fp32=False,
             exam_seeds=EXAM_SEEDS, per_node=32, futures=16, max_root_seeds=4096):
    source = load_policy(baseline, device, fp32)
    runner = collector(lanes, device, seed, deadline, storage_slots=len(NODES) * per_node,
                       cls=RootCollector)
    roots = runner.capture(source, per_node, max_root_seeds)
    del source
    results = []
    for path in paths:
        model = load_policy(path, device, fp32)
        runner.search.rng.manual_seed(seed)
        runner.search.greedy = True
        argmax = runner.evaluate(model, exam_seeds)
        runner.search.greedy = False
        runner.search.rng.manual_seed(seed)
        sample = runner.evaluate(model, exam_seeds)
        exam = exam_comparison(sample, argmax)
        runner.search.rng.manual_seed(seed + 1)
        nodes, decisions = remaining_returns(runner, model, roots, futures, seed + 2)
        results.append(dict(checkpoint=str(path), exam=exam, nodes=nodes, decisions=decisions))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoints", nargs="+", type=Path)
    parser.add_argument("--dev-112", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="New directory for JSON and Markdown")
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=512)
    parser.add_argument("--seed", type=int, default=101)
    parser.add_argument("--fp32", action="store_true", help="Use the fp32 exam path for fp32 reference checkpoints")
    args = parser.parse_args()
    if not 1 <= len(args.checkpoints) <= 3 or args.lanes < 1:
        parser.error("Supply one to three checkpoints and a positive lane count")
    torch.set_num_threads(1)
    device = torch.device(args.device)
    started = time.monotonic()
    args.output.mkdir()
    results = diagnose(args.checkpoints, args.dev_112, device, args.lanes, args.seed, started + 10800, fp32=args.fp32)
    payload = dict(seed=args.seed, lanes=args.lanes, device=str(device), fp32=args.fp32,
                   elapsed_seconds=time.monotonic() - started, results=results)
    (args.output / "value-truth.json").write_text(json.dumps(payload, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    (args.output / "value-truth.md").write_text(summary(results), encoding="utf-8")


if __name__ == "__main__":
    main()
