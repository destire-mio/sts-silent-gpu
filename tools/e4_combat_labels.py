"""Resumable train-seed boss/elite CRN labels with a measured wall-clock budget."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import random
import statistics
import time

import torch

from tools.e4_map_labels import atomic_json, crn_diagnostics, label_options, pack_observations, read_json
from tools.e4_stage_exam import replay_prefixes
from tools.e4_value_residual import training_seeds
from tools.e4_value_truth import load_policy
from train.e4 import ACT, AllocationEnded, BOSS_ROOM, COMBAT, COMBAT_PHASE, DEATH, ELITE_ROOM, FLOOR, HP, PHASE, ROOM, SCREEN, VICTORY
from train.e4_stage12 import NoSLCollector, NoSLSearch
from train.model import network_inputs

FORMAT = "e4_combat_labels_v1"
INITIAL_DRAWS, EXTRA_DRAWS, MAX_STEPS = 8, 16, 300


def balanced_roots(episodes, seed):
    buckets = defaultdict(list)
    for episode in episodes:
        for root in episode["roots"]:
            buckets[root["act"]].append(dict(root, seed=episode["seed"],
                path=episode["path"][:root["prefix_length"]]))
    rng = random.Random(seed)
    for rows in buckets.values():
        rng.shuffle(rows)
    result = []
    while any(buckets.values()):
        for act in sorted(buckets):
            if buckets[act]:
                result.append(buckets[act].pop())
    return result


def combat_root(progress):
    return (progress[SCREEN] == COMBAT and progress[PHASE] == COMBAT_PHASE
            and progress[ROOM] in (BOSS_ROOM, ELITE_ROOM))


def policy_actions(_search, _slot, mask):
    return torch.where(mask)[0]


def capture_combat_episodes(model, runner, seeds):
    return capture_label_episodes(model, runner, seeds, is_root=combat_root, options_at=policy_actions,
                                  point_limit=4, allow_partial=True)


@torch.no_grad()
def capture_label_episodes(model, runner, seeds, *, is_root, options_at, point_limit=None, allow_partial=False):
    # Label replay may have left fixed roots live in the reusable runner.
    runner.seeds[:] = [None] * len(runner.seeds)
    waiting = iter(seeds)
    pending = [[] for _ in runner.seeds]
    counts = [Counter() for _ in runner.seeds]
    episodes = []
    captured_logits = None

    def capture_logits(_module, _inputs, outputs):
        nonlocal captured_logits
        captured_logits = outputs[0]

    hook = model.register_forward_hook(capture_logits)
    try:
        while True:
            runner.search.clock()
            free = [slot for slot, seed in enumerate(runner.seeds) if seed is None]
            fresh = [seed for _, seed in zip(free, waiting)]
            if fresh:
                runner.reset(free[:len(fresh)], fresh)
                for slot in free[:len(fresh)]:
                    pending[slot].clear()
                    counts[slot].clear()
            live = [slot for slot, seed in enumerate(runner.seeds) if seed is not None]
            if not live:
                break
            before = runner.search.progress_for(live)
            captured_logits = None
            actions, _, _, _ = runner.choose(model, live, before)
            healthy = [i for i, slot in enumerate(live) if slot not in runner.search.failed]
            for position, i in enumerate(healthy):
                slot, p = live[i], before[live[i]]
                if not is_root(p):
                    continue
                fight = (p[ACT], p[FLOOR], p[ROOM], bool(runner.rewards[slot].reach["act3_first_boss"]))
                legal = options_at(runner.search, slot, runner.search.last_policy_mask[i])
                if len(legal) < 2 or (point_limit is not None and counts[slot][fight] >= point_limit):
                    continue
                logits = captured_logits[position, legal].float()
                order = torch.argsort(logits, descending=True, stable=True)[:4]
                options = legal[order].tolist()
                prefix = len(runner.paths[slot])
                identity = hashlib.sha256(json.dumps([runner.seeds[slot], runner.paths[slot]]).encode()).hexdigest()
                pending[slot].append(dict(id=identity, act=p[ACT], floor=p[FLOOR], fight=list(fight),
                    progress=p, prefix_length=prefix, options=options, argmax=options[0],
                    p_ref_logits=logits[order].tolist(), p_ref_all_options=legal.tolist(),
                    p_ref_all_logits=logits.tolist()))
                counts[slot][fight] += 1
            previous = len(runner.completed)
            _, terminal = runner.advance(live, actions, before)
            for slot, episode in zip((s for s, done in zip(live, terminal) if done), runner.completed[previous:]):
                episodes.append(dict(seed=episode["seed"], path=episode["candidates"],
                                     outcome=episode["outcome"], roots=pending[slot].copy()))
            runner.completed.clear()
    except AllocationEnded:
        if not allow_partial:
            raise
    finally:
        hook.remove()
    return episodes


@torch.no_grad()
def combat_rollout(search, model, plan):
    slots = search.take(len(plan))
    parents, first, seeds = zip(*plan)
    result = dict.fromkeys(slots)
    decisions = 0
    try:
        search.clone(list(parents), slots)
        search.batch.determinize_slots(slots, list(seeds), True)
        live = slots.copy()
        for tick in range(MAX_STEPS):
            if not live:
                break
            search.clock()
            if tick == 0:
                choices = [first[slots.index(slot)] for slot in live]
            else:
                inputs = network_inputs(search.arrays, search.device, torch.tensor(live, device=search.device))
                logits = model(inputs)[0].masked_fill(~inputs["candidate_mask"], -torch.inf)
                choices = logits.argmax(-1).tolist()
            search.batch.step_slots(live, choices)
            decisions += len(live)
            progress = search.progress_for(live)
            faults = search.arrays["faults"].long()[live].tolist()
            truncated = search.arrays["truncated"][live].tolist()
            remaining, alive_end = [], []
            for slot, fault, cut in zip(live, faults, truncated):
                p = progress[slot]
                if fault or cut:
                    continue
                if p[HP] <= 0 or p[SCREEN] == DEATH:
                    result[slot] = 0.
                elif p[PHASE] != COMBAT_PHASE or p[SCREEN] == VICTORY:
                    alive_end.append(slot)
                else:
                    remaining.append(slot)
            if alive_end:
                inputs = network_inputs(search.arrays, search.device, torch.tensor(alive_end, device=search.device))
                for slot, value in zip(alive_end, model(inputs)[1].tolist()):
                    result[slot] = value if math.isfinite(value) else None
            live = remaining
        # Still-live 300-step futures are incomplete, not synthetic value labels.
        return [result[slot] for slot in slots], decisions
    finally:
        for slot in slots:
            search.mainline_progress.pop(slot, None)
        search.available.extend(slots)


def evaluate_draws(runner, model, samples, targets, rollout=combat_rollout):
    plan = [(slot, action, seed) for slot, actions, seeds in targets for action in actions for seed in seeds]
    capacity = len(runner.search.available)
    decisions = 0
    for start in range(0, len(plan), capacity):
        chunk = plan[start:start + capacity]
        scores, steps = rollout(runner.search, model, chunk)
        decisions += steps
        for (slot, action, _), score in zip(chunk, scores):
            samples[slot][action].append(score)
    return decisions


def label_combat_shard(runner, model, records, seed):
    return label_shard(runner, model, records, seed, label_format=FORMAT, draw_namespace="combat",
                       rollout=combat_rollout)


@torch.no_grad()
def label_shard(runner, model, records, seed, *, label_format, draw_namespace, rollout):
    started = time.monotonic()
    slots = replay_prefixes(runner, records)
    progress = runner.search.progress_for(slots)
    if any(progress[slot] != row["progress"] for slot, row in enumerate(records)):
        raise ValueError("Label root changed on replay")
    observations = pack_observations(runner.search, slots)
    samples = [{action: [] for action in row["options"]} for row in records]
    draws = []
    for row in records:
        rng = random.Random(f"{draw_namespace}:{seed}:{row['id']}")
        draws.append([rng.randrange(2**63) for _ in range(INITIAL_DRAWS + EXTRA_DRAWS)])
    steps = evaluate_draws(runner, model, samples, [
        (slot, row["options"], draws[slot][:INITIAL_DRAWS]) for slot, row in enumerate(records)], rollout)
    retained = {}
    for slot, values in enumerate(samples):
        if all(None not in scores for scores in values.values()):
            retained[slot] = sorted(values, key=lambda action: statistics.mean(values[action]), reverse=True)[:math.ceil(len(values) / 2)]
    steps += evaluate_draws(runner, model, samples, [
        (slot, list(dict.fromkeys(kept + [records[slot]["argmax"]])), draws[slot][INITIAL_DRAWS:])
        for slot, kept in retained.items()], rollout)
    rows = []
    for slot, root in enumerate(records):
        values = samples[slot]
        complete = slot in retained and all(None not in scores for scores in values.values())
        rows.append(dict(root, complete=complete, packed_observation=observations[slot], scores=values,
            draws=draws[slot], estimates=label_options(values, root["argmax"], retained.get(slot, []))))
    return dict(format=label_format, rows=rows, seconds=time.monotonic() - started, preview_steps=steps)


def label_parser(description):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", nargs="+", default=["1000000000:1000004096"])
    parser.add_argument("--labels", type=int, help="Optional upper limit; otherwise measured after the pilot")
    parser.add_argument("--pilot-seconds", type=float, default=1800.)
    parser.add_argument("--max-seconds", type=float, default=10800.)
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--source-shard", type=int, default=16)
    parser.add_argument("--shard-size", type=int, default=8)
    parser.add_argument("--preview-lanes", type=int, default=256)
    parser.add_argument("--seed", type=int, default=203)
    return parser


def validate_label_arguments(parser, args):
    if (not 0 < args.pilot_seconds <= args.max_seconds <= 10800 or args.preview_lanes < 1
            or min(args.source_shard, args.shard_size) < 1 or (args.labels is not None and args.labels < 1)):
        parser.error("Require positive sizes and 0 < pilot <= total <= 10800 seconds")


def run_label_job(args, *, label_format, capture, label, manifest_settings, value_target,
                  pilot_metrics=None, search_type=NoSLSearch):
    args.output.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(1)
    seeds = training_seeds(args.seeds)
    with args.checkpoint.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    manifest = dict(format=label_format, checkpoint_sha256=digest, seeds=seeds, seed=args.seed,
                    source_shard=args.source_shard, shard_size=args.shard_size, preview_lanes=args.preview_lanes,
                    pilot_seconds=args.pilot_seconds, max_seconds=args.max_seconds, labels_limit=args.labels,
                    device=args.device, initial_draws=INITIAL_DRAWS, extra_draws=EXTRA_DRAWS, **manifest_settings)
    file = args.output / "manifest.json"
    if file.exists() and read_json(file) != manifest:
        raise ValueError("Resume requires the same checkpoint, seeds, layout and budget")
    atomic_json(file, manifest)
    budget_file = args.output / "budget.json"
    budget = read_json(budget_file) if budget_file.exists() else dict(elapsed=0., active_since=None)
    if budget["active_since"] is not None:
        budget["elapsed"] += max(0., time.time() - budget["active_since"])
    budget["active_since"] = time.time()
    atomic_json(budget_file, budget)
    started = time.monotonic()
    deadline = started + max(0., args.max_seconds - budget["elapsed"])
    device = torch.device(args.device)
    model = load_policy(args.checkpoint, device)
    search = search_type(max(args.source_shard, args.shard_size), device, args.seed,
                         greedy=True, storage_slots=args.preview_lanes, probe_slots=128)
    search.deadline, search.evidence_directory = deadline, args.output
    runner = NoSLCollector(search, torch.Generator().manual_seed(args.seed))
    shards = [read_json(file) for file in sorted(args.output.glob("labels-*.json.gz"))]
    seen = {row["id"] for shard in shards for row in shard["rows"]}
    target = args.labels
    pilot_file = args.output / "pilot.json"
    pilot = read_json(pilot_file) if pilot_file.exists() else None
    if pilot is not None:
        target = min(target, pilot["planned_points"]) if target else pilot["planned_points"]
    def record_pilot(elapsed, completed):
        planned = math.floor(completed / elapsed * args.max_seconds)
        report = dict(seconds=elapsed, completed_points=completed, points_per_second=completed / elapsed,
                      planned_points=planned, total_budget_seconds=args.max_seconds, device=args.device)
        if pilot_metrics is not None:
            report.update(pilot_metrics([row for shard in shards for row in shard["rows"]]))
        atomic_json(pilot_file, report)
        return report

    source_seconds = 0.
    try:
        for offset in range(0, len(seeds), args.source_shard):
            search.clock()
            source = args.output / f"source-{offset:07d}.json.gz"
            if source.exists():
                saved = read_json(source)
            else:
                capture_started = time.monotonic()
                episodes = capture(model, runner, seeds[offset:offset + args.source_shard])
                saved = dict(episodes=episodes, seconds=time.monotonic() - capture_started)
                atomic_json(source, saved)
            source_seconds += saved["seconds"]
            roots = [row for row in balanced_roots(saved["episodes"], args.seed + offset) if row["id"] not in seen]
            for start in range(0, len(roots), args.shard_size):
                search.clock()
                completed = sum(row["complete"] for shard in shards for row in shard["rows"])
                elapsed = budget["elapsed"] + time.monotonic() - started
                if pilot is None and completed > 0 and elapsed >= args.pilot_seconds:
                    pilot = record_pilot(elapsed, completed)
                    planned = pilot["planned_points"]
                    target = min(target, planned) if target else planned
                if target is not None and completed >= target:
                    break
                selected = roots[start:start + args.shard_size]
                if target is not None:
                    selected = selected[:max(0, target - completed)]
                if not selected:
                    break
                shard = label(runner, model, selected, args.seed)
                atomic_json(args.output / f"labels-{len(shards):07d}.json.gz", shard)
                shards.append(shard)
                seen.update(row["id"] for row in shard["rows"])
            if target is not None and sum(row["complete"] for shard in shards for row in shard["rows"]) >= target:
                break
    except AllocationEnded:
        pass
    finally:
        budget["elapsed"] += time.monotonic() - started
        budget["active_since"] = None
        atomic_json(budget_file, budget)
    rows = [row for shard in shards for row in shard["rows"]]
    complete = sum(row["complete"] for row in rows)
    if pilot_metrics is not None and pilot is None and complete and budget["elapsed"] >= args.pilot_seconds:
        pilot = record_pilot(budget["elapsed"], complete)
    summary = dict(format=label_format, attempted_points=len(rows), actual_complete_points=complete,
        actual_wall_seconds=budget["elapsed"], pilot=pilot, source_seconds=source_seconds,
        labeling_seconds=sum(shard["seconds"] for shard in shards), preview_steps=sum(shard["preview_steps"] for shard in shards),
        complete_by_act=dict(Counter(row["act"] for row in rows if row["complete"])),
        crn_by_act=crn_diagnostics(rows, "labels"), max_steps=manifest_settings["max_steps"],
        value_target=value_target, determinize_run=True)
    if pilot_metrics is not None:
        summary.update(pilot_metrics(rows))
    atomic_json(args.output / "summary.json", summary)
    print(json.dumps(summary, indent=2))


def main(argv=None):
    parser = label_parser(__doc__)
    args = parser.parse_args(argv)
    validate_label_arguments(parser, args)
    run_label_job(args, label_format=FORMAT, capture=capture_combat_episodes, label=label_combat_shard,
                  manifest_settings=dict(max_steps=MAX_STEPS, maximum_points_per_fight=4),
                  value_target="V after this combat; death=0; capped futures excluded")


if __name__ == "__main__":
    main()
