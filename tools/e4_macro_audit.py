"""Audit macro-action counterfactuals with paired determinized continuations."""
import argparse
from copy import deepcopy
import json
from pathlib import Path
import random
import time

import numpy as np
import torch

from tools.e4_value_truth import collector, load_policy, mean_se
from train.e4 import ACT, AllocationEnded, DEATH, FLOOR, MAP, MAX_REWARD, ROOM, ROOM_SCREEN, SCREEN, VICTORY
from train.e4_evaluation_ledger import spearman
from train.e4_stage12 import NoSLCollector
from train.model import network_inputs


COUNTS = {"card": 96, "map": 64, "campfire": 48, "purge": 48}
STAGES = {"k8": 8, "k32": 32, "k128": 128, "k32_independent": 32}
BOSS_REACH = {1: "act1_boss", 2: "act2_boss", 3: "act3_second_boss"}
CHOOSE, PROCEED, SKIP = 3, 6, 7


def root_kind(progress, screen_auxiliary):
    screen, room = progress[SCREEN], progress[ROOM]
    if screen == 6:
        return "card"
    if screen == MAP:
        return "map"
    if screen == ROOM_SCREEN and room == 4:
        return "campfire"
    if screen == 2 and room == 3 and screen_auxiliary == 2:
        return "purge"
    return None


def option_rows(kind, legal, actions, refs, tags, ids):
    options = []
    for action, valid in enumerate(legal):
        if not valid:
            continue
        code, ref = actions[action], refs[action]
        tag, entity = (tags[ref-1], ids[ref-1]) if ref else (0, 0)
        if kind == "card" and code in (CHOOSE, SKIP):
            options.append(action)
        elif kind == "map" and code == CHOOSE and tag == 10:
            options.append(action)
        elif kind == "campfire" and code == CHOOSE and (tag, entity) == (11, 5):
            options.append(action)
        elif kind == "purge" and (code == SKIP or code == CHOOSE and tag == 3):
            options.append(action)
    return options


class MacroRoots(NoSLCollector):
    @torch.no_grad()
    def capture(self, model, quotas, max_seeds):
        search = self.search
        waiting = iter(range(2_300_000_000, 2_300_000_000 + max_seeds))
        accepted = {key: set() for key in quotas}
        roots = []
        while any(len(accepted[key]) < count for key, count in quotas.items()):
            search.clock()
            free = [slot for slot, seed in enumerate(self.seeds) if seed is None]
            seeds = [seed for _, seed in zip(free, waiting)]
            if seeds:
                self.reset(free[:len(seeds)], seeds)
            live = [slot for slot, seed in enumerate(self.seeds) if seed is not None]
            assert live, {key: (len(accepted[key]), count) for key, count in quotas.items()}
            progress = search.progress_for(live)
            actions, _, _, _ = self.choose(model, live, progress)
            candidate = [slot for slot in live if slot not in search.failed
                         and progress[slot][ACT] in BOSS_REACH
                         and progress[slot][SCREEN] in (2, 6, MAP, ROOM_SCREEN)
                         and not self.rewards[slot].reach[BOSS_REACH[progress[slot][ACT]]]]
            if candidate:
                batch = network_inputs(search.arrays, search.device, torch.tensor(candidate, device=search.device))
                raw_mask = batch["candidate_mask"].tolist()
                tags = batch["tags"][:, 0].tolist()
                ids = batch["ids"][:, 0].tolist()
                aux = batch["tags"][:, 3, 0].tolist()
                kinds = batch["candidate_kinds"].tolist()
                refs = batch["candidate_refs"][:, 2].tolist()
                ranked = []
                for j, slot in enumerate(candidate):
                    act = progress[slot][ACT]
                    kind = root_kind(progress[slot], aux[j])
                    key = (kind, act)
                    seed = self.seeds[slot]
                    if key not in quotas or len(accepted[key]) >= quotas[key] or seed in accepted[key]:
                        continue
                    options = option_rows(kind, raw_mask[j], kinds[j], refs[j], tags[j], ids[j])
                    if len(options) < 2 or kind == "purge" and not any(kinds[j][a] == SKIP for a in options):
                        continue
                    ranked.append((j, slot, key, options))
                    accepted[key].add(seed)
                if ranked:
                    selected = [j for j, _, _, _ in ranked]
                    logits = model({name: value[selected] for name, value in batch.items()})[0].tolist()
                    for row, (j, slot, (kind, act), options) in zip(logits, ranked):
                        target, = search.take(1)
                        search.clone([slot], [target])
                        roots.append(dict(slot=target, seed=self.seeds[slot], kind=kind, act=act,
                                          progress=progress[slot], path=self.paths[slot].copy(),
                                          ledger=deepcopy(self.rewards[slot]), options=options,
                                          no_purge=next((a for a in options if kinds[j][a] == SKIP), None)
                                          if kind == "purge" else None,
                                          argmax=max(options, key=row.__getitem__),
                                          timings={}, trials={}))
            self.advance(live, actions, progress)
            self.completed.clear()
        return roots


def future_uniforms(seeds, count):
    # Separate engine and policy streams, shared across the options of one root/draw.
    draws = {seed: np.random.default_rng(seed ^ 0x6A09E667F3BCC909).random(count)
             for seed in set(seeds)}
    return np.stack([draws[seed] for seed in seeds])


@torch.no_grad()
def continuation_actions(search, model, slots, progress, uniform, confirm, no_purge):
    search.mainline_progress = dict(progress)
    mask = search.policy_masks(slots)
    inputs = network_inputs(search.arrays, search.device, torch.tensor(slots, device=search.device))
    legal = inputs["candidate_mask"]
    legal &= mask[:, :legal.shape[1]]
    refs = inputs["candidate_refs"][:, 2]
    targets = (refs - 1).clamp_min(0)
    is_purge = ((refs > 0) & (inputs["tags"][:, 0].gather(1, targets) == 11)
                & (inputs["ids"][:, 0].gather(1, targets) == 2))
    suppress = torch.tensor(no_purge, device=search.device)[:, None] & is_purge
    legal &= ~suppress
    confirm_rows = torch.tensor(confirm, device=search.device)[:, None]
    legal &= ~confirm_rows | (inputs["candidate_kinds"] == PROCEED)
    assert bool(legal.any(-1).all()), "Macro purge branch lost its continuation"
    logits = model(inputs)[0]
    cdf = logits.float().softmax(dim=-1).cumsum(dim=-1)
    total = cdf[:, -1:]
    draw = torch.minimum(uniform[:, None].float()*total, torch.nextafter(total, torch.zeros_like(total)))
    return (cdf <= draw).sum(dim=-1).tolist()


@torch.no_grad()
def run_stage(runner, model, roots, name, count, rng, limit):
    search = runner.search
    plan = []
    for index, root in enumerate(roots):
        seeds = [rng.randrange(2**63) for _ in range(count)]
        root["trials"][name] = dict(seeds=seeds, scores={str(a): [None] * count for a in root["options"]})
        root["timings"][name] = dict(decisions=0, allocated_wall_seconds=0., truncated=0, faults=0)
        for action in root["options"]:
            plan.extend((index, action, draw, seed) for draw, seed in enumerate(seeds))
    for start in range(0, len(plan), search.lanes):
        search.clock()
        started = time.monotonic()
        chunk = plan[start:start+search.lanes]
        slots = list(range(len(chunk)))
        sources = [roots[index] for index, _, _, _ in chunk]
        runner.reset(slots, [root["seed"] for root in sources])
        search.clone([root["slot"] for root in sources], slots)
        random_seeds = [seed for _, _, _, seed in chunk]
        search.batch.determinize_slots(slots, random_seeds, True)
        search.check(slots)
        active = [slot for slot in slots if slot not in search.failed]
        for slot, key in zip(active, search.loop_keys(active) if active else []):
            search.seen[slot].add(key)
            search.mask_cache.pop(slot, None)
            search.repeat_blocked.pop(slot, None)
        for slot, root in enumerate(sources):
            runner.rewards[slot] = deepcopy(root["ledger"])
            if slot in search.failed:
                root["timings"][name]["faults"] += 1
        uniforms = torch.as_tensor(future_uniforms(random_seeds, limit), device=search.device)
        steps = [0] * len(slots)
        for tick in range(limit):
            if not active:
                break
            search.clock()
            before = search.progress_for(active)
            if tick == 0:
                actions = [chunk[s][1] for s in active]
            else:
                confirm = [tick == 1 and sources[s]["kind"] == "purge"
                           and chunk[s][1] != sources[s]["no_purge"] for s in active]
                no_purge = [sources[s]["kind"] == "purge" and chunk[s][1] == sources[s]["no_purge"]
                            and before[s][FLOOR] == sources[s]["progress"][FLOOR]
                            and before[s][SCREEN] == 11 for s in active]
                actions = continuation_actions(search, model, active, before, uniforms[active, tick], confirm, no_purge)
            runner.stalled = set()
            _, terminal = runner.advance(active, actions, before)
            runner.completed.clear()
            after = search.last_step_progress
            remaining = []
            for slot, ended in zip(active, terminal):
                root = sources[slot]
                steps[slot] += 1
                if slot in runner.stalled:
                    root["timings"][name]["faults"] += 1
                elif (ended or after[slot][SCREEN] in (DEATH, VICTORY)
                      or after[slot][ACT] != root["act"] or runner.rewards[slot].reach[BOSS_REACH[root["act"]]]):
                    _, action, draw, _ = chunk[slot]
                    root["trials"][name]["scores"][str(action)][draw] = (
                        (runner.rewards[slot].total - root["ledger"].total) / MAX_REWARD)
                else:
                    remaining.append(slot)
            active = remaining
        if active:
            inputs = network_inputs(search.arrays, search.device, torch.tensor(active, device=search.device))
            for slot, value in zip(active, model(inputs)[1].tolist()):
                root = sources[slot]
                _, action, draw, _ = chunk[slot]
                root["trials"][name]["scores"][str(action)][draw] = (
                    (runner.rewards[slot].total - root["ledger"].total) / MAX_REWARD + value)
                root["timings"][name]["truncated"] += 1
        elapsed = time.monotonic() - started
        total_steps = sum(steps)
        denominator = total_steps or len(steps)
        for slot, count_steps in enumerate(steps):
            timing = sources[slot]["timings"][name]
            timing["decisions"] += count_steps
            timing["allocated_wall_seconds"] += elapsed * (count_steps if total_steps else 1) / denominator


def stage_stats(root, name):
    trials = root["trials"].get(name)
    if trials is None:
        return None
    scores = trials["scores"]
    paired = [i for i in range(len(trials["seeds"])) if all(row[i] is not None for row in scores.values())]
    if len(paired) < 2:
        return None
    means = {action: mean_se([scores[str(action)][i] for i in paired])[0] for action in root["options"]}
    best = max(root["options"], key=means.__getitem__)
    argmax = root["argmax"]
    gain, error = mean_se([scores[str(best)][i] - scores[str(argmax)][i] for i in paired])
    return dict(paired=len(paired), best=best, means=[means[a] for a in root["options"]],
                gain=gain, paired_SE=error, exceeds_1se=gain > error, exceeds_2se=gain > 2*error)


def summarize(roots, stages):
    report = {}
    for kind in COUNTS:
        group = [root for root in roots if root["kind"] == kind]
        by_stage, stability = {}, {}
        for name in stages:
            rows = [root["statistics"][name] for root in group if root["statistics"][name] is not None]
            gain, se = mean_se([row["gain"] for row in rows])
            by_stage[name] = dict(roots=len(rows), gain=gain, root_SE=se,
                                  regret_1se=sum(row["exceeds_1se"] for row in rows),
                                  regret_2se=sum(row["exceeds_2se"] for row in rows))
        for first, second in (("k8","k32"),("k8","k128"),("k32","k128"),("k32","k32_independent")):
            pairs = [(root["statistics"].get(first), root["statistics"].get(second)) for root in group]
            pairs = [(a, b) for a, b in pairs if a is not None and b is not None]
            correlation = [spearman(list(zip(a["means"], b["means"]))) for a, b in pairs]
            valid = [score for score in correlation if score is not None]
            stability[f"{first}_vs_{second}"] = dict(roots=len(pairs),
                best_agree=sum(a["best"] == b["best"] for a, b in pairs),
                spearman=mean_se(valid)[0], spearman_roots=len(valid))
        report[kind] = dict(roots=len(group), roots_by_act={str(a):sum(r["act"] == a for r in group) for a in BOSS_REACH},
                            stages=by_stage, stability=stability,
                            decisions=sum(t["decisions"] for r in group for t in r["timings"].values()),
                            allocated_wall_seconds=sum(t["allocated_wall_seconds"] for r in group for t in r["timings"].values()))
    return report


def markdown(result):
    fmt = lambda x: "NA" if x is None else f"{x:.6f}"
    lines = ["# E4 macro-action audit", "",
             f"Complete {int(result['complete'])} · roots {len(result['roots'])} · seconds {result['seconds']:.2f}", "",
             "| Type | Draws | Paired roots | Regret >1SE | Regret >2SE | Gain | Root SE |",
             "|---|---|---:|---:|---:|---:|---:|"]
    for kind, group in result["summary"].items():
        for name, row in group["stages"].items():
            lines.append(f"| {kind} | {name} | {row['roots']} | {row['regret_1se']}/{row['roots']} | "
                         f"{row['regret_2se']}/{row['roots']} | {fmt(row['gain'])} | {fmt(row['root_SE'])} |")
    lines += ["", "| Type | Comparison | Same best | Mean Spearman | Spearman roots |",
              "|---|---|---:|---:|---:|"]
    for kind, group in result["summary"].items():
        for pair, row in group["stability"].items():
            lines.append(f"| {kind} | {pair} | {row['best_agree']}/{row['roots']} | "
                         f"{fmt(row['spearman'])} | {row['spearman_roots']} |")
    lines += ["", "| Type | Decisions | Allocated wall seconds |", "|---|---:|---:|"]
    for kind, group in result["summary"].items():
        lines.append(f"| {kind} | {group['decisions']} | {group['allocated_wall_seconds']:.3f} |")
    return "\n".join(lines) + "\n"


def audit(model, device, lanes, seed, deadline, *, quotas, stages=STAGES, limit=600, max_seeds=30000):
    started = time.monotonic()
    runner = collector(lanes, device, seed, deadline, storage_slots=sum(quotas.values()), cls=MacroRoots)
    roots = runner.capture(model, quotas, max_seeds)
    rng = random.Random(seed)
    complete = True
    try:
        for name, count in stages.items():
            run_stage(runner, model, roots, name, count, rng, limit)
    except AllocationEnded:
        complete = False
    for root in roots:
        root["statistics"] = {name:stage_stats(root, name) for name in stages}
    report = summarize(roots, stages)
    output = [{key:value for key, value in root.items() if key not in ("ledger","slot")} for root in roots]
    return dict(complete=complete, seconds=time.monotonic()-started, seed=seed, cutoff=limit,
                summary=report, roots=output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=1024)
    parser.add_argument("--seed", type=int, default=101)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    if args.lanes < 1:
        parser.error("--lanes must be positive")
    deadline = time.monotonic() + 3*3600
    device = torch.device(args.device)
    if device.type == "cuda":
        torch.cuda.set_device(device)
    torch.set_num_threads(1)
    model = load_policy(args.checkpoint, device, args.fp32)
    quotas = {(kind, act):count//3+int(act <= count%3)
              for kind, count in COUNTS.items() for act in BOSS_REACH}
    args.output.mkdir(parents=True, exist_ok=False)
    result = audit(model, device, args.lanes, args.seed, deadline, quotas=quotas)
    result["checkpoint"] = str(args.checkpoint)
    (args.output/"macro-audit.json").write_text(json.dumps(result, allow_nan=False)+"\n", encoding="utf-8")
    (args.output/"macro-audit.md").write_text(markdown(result), encoding="utf-8")
    if not result["complete"]:
        raise SystemExit("The audit reached its three-hour limit; partial results were saved.")


if __name__ == "__main__":
    main()
