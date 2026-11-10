"""Card-reward horizons evaluated on one shared set of sampled continuations."""
from copy import deepcopy
import hashlib
import math
import platform
import random
import time

import torch

from tools.e4_map_labels import atomic_json, capture_episodes, read_json, replay, stratified_roots
from tools.e4_value_residual import CARD_TYPES, card_catalog
from tools.e4_value_truth import collector, mean_se
from train.e4 import ACT, AllocationEnded, MAX_REWARD
from train.e4_lookahead import MapLookahead
from train.model import network_inputs

HORIZONS = ("act", "next_act", "full")
CARD_REWARD, CHOOSE, SKIP = 6, 3, 7
PILOT_SECONDS, AUDIT_SECONDS = 1800, 21600


def card_options(search, slots, mask):
    kinds = search.arrays["candidate_kinds"][slots]
    legal = mask[:, :kinds.shape[1]]
    choices = legal & (kinds == CHOOSE)
    skip = legal & (kinds == SKIP)
    # A compulsory in-combat discovery has no skip and is not a card reward.
    return [torch.where(row)[0].tolist() if has_skip else []
            for row, has_skip in zip(choices | skip, skip.any(dim=-1).tolist())]


def card_identities(search, slots, roots):
    refs = search.arrays["candidate_refs"][slots, 2].long()
    tags = search.arrays["tags"][slots, 0].long().gather(1, (refs - 1).clamp_min(0))
    ids = search.arrays["ids"][slots, 0].long().gather(1, (refs - 1).clamp_min(0))
    ids = ids.masked_fill((refs == 0) | (tags != 3), 0).tolist()
    for row, root in zip(ids, roots):
        root["card_ids"] = {action: row[action] for action in root["options"]}


def reached_horizon(horizon, start_act, after_act, ledger, terminal):
    if terminal:
        return True
    target = start_act + (horizon == "next_act")
    return (horizon != "full" and target in MapLookahead.boss_reach and
            (after_act > target or ledger.reach[MapLookahead.boss_reach[target]]))


@torch.no_grad()
def score_chunk(runner, model, roots, chunk, horizons, future_uniforms, continuation_actions):
    search = runner.search
    slots = list(range(len(chunk)))
    sources = [roots[index] for index, _, _, _ in chunk]
    runner.reset(slots, [root["seed"] for root in sources])
    search.clone([root["slot"] for root in sources], slots)
    seeds = [seed for _, _, _, seed in chunk]
    search.batch.determinize_slots(slots, seeds, True)
    search.check(slots)
    active = [s for s in slots if s not in search.failed]
    for slot, key in zip(active, search.loop_keys(active) if active else []):
        search.seen[slot].add(key)
        search.mask_cache.pop(slot, None)
        search.repeat_blocked.pop(slot, None)
        runner.rewards[slot] = deepcopy(sources[slot]["ledger"])
    for slot in search.failed.keys() & set(slots):
        sources[slot]["faults"] += 1
    pending = [set(horizons) for _ in slots]
    uniforms = torch.as_tensor(future_uniforms(seeds, 512), device=search.device)
    tick = 0
    while active:
        search.clock()
        before = search.progress_for(active)
        if tick == 0:
            actions = [chunk[s][1] for s in active]
        else:
            if tick >= uniforms.shape[1]:
                uniforms = torch.as_tensor(future_uniforms(seeds, 2 * tick), device=search.device)
            actions = continuation_actions(search, model, active, before, uniforms[active, tick],
                                           [False] * len(active), [False] * len(active))
        runner.stalled = set()
        _, terminal = runner.advance(active, actions, before)
        runner.completed.clear()
        after = search.last_step_progress
        snapshot = {}
        continuing = []
        for slot, done in zip(active, terminal):
            root = sources[slot]
            root["steps"] += 1
            if slot in runner.stalled:
                root["faults"] += 1
                continue
            ledger = runner.rewards[slot]
            finished = [name for name in horizons if name in pending[slot] and
                        reached_horizon(name, root["act"], after[slot][ACT], ledger, done)]
            if finished:
                _, action, draw, _ = chunk[slot]
                reward = ledger.total - root["ledger"].total
                if done:
                    for name in finished:
                        root["scores"][name][action][draw] = reward
                else:
                    snapshot[slot] = (root, finished, action, draw, reward)
                pending[slot].difference_update(finished)
            if not done and pending[slot]:
                continuing.append(slot)
        if snapshot:
            slots_at_boundary = torch.tensor(list(snapshot), device=search.device)
            inputs = network_inputs(search.arrays, search.device, slots_at_boundary)
            for (root, names, action, draw, reward), value in zip(snapshot.values(), model(inputs)[1].tolist()):
                for name in names:
                    root["scores"][name][action][draw] = reward + value * MAX_REWARD
        active = continuing
        tick += 1


def audit_batch(runner, model, records, horizons, count, seed, future_uniforms, continuation_actions):
    search = runner.search
    started = time.monotonic()
    slots, _ = replay(runner, records, options_at=card_options)
    targets = search.take(len(slots))
    roots = [dict(record, slot=target, ledger=deepcopy(runner.rewards[slot]), steps=0, faults=0)
             for record, slot, target in zip(records, slots, targets)]
    complete = True
    try:
        card_identities(search, slots, roots)
        search.clone(slots, targets)
        plan = []
        for index, root in enumerate(roots):
            rng = random.Random(f"{seed}:{root['seed']}:{root['prefix_length']}")
            draws = [rng.randrange(2**63) for _ in range(count)]
            root["draws"] = draws
            root["scores"] = {name: {action: [None] * count for action in root["options"]} for name in horizons}
            for action in root["options"]:
                plan.extend((index, action, i, draw) for i, draw in enumerate(draws))
        try:
            for start in range(0, len(plan), search.lanes):
                score_chunk(runner, model, roots, plan[start:start+search.lanes], horizons,
                            future_uniforms, continuation_actions)
        except AllocationEnded:
            complete = False
    finally:
        search.available.extend(targets)
    rows = [{k: v for k, v in root.items() if k not in ("slot", "ledger")} for root in roots]
    return dict(complete=complete, seconds=time.monotonic() - started, roots=rows)


def heldout_gain(scores, argmax):
    count = len(scores[argmax])
    if any(None in row for row in scores.values()):
        return None
    half = count // 2
    best = max([argmax] + list(scores), key=lambda action: mean_se(scores[action][:half])[0])
    gain, se = mean_se([a - b for a, b in zip(scores[best][half:], scores[argmax][half:])])
    return dict(best=best, gain=gain, paired_se=se, heldout_draws=half)


def stats(values):
    mean, se = mean_se(values)
    return dict(roots=len(values), mean=mean, se=se)


def summarize(rows, horizons, catalog):
    report = {}
    for name in horizons:
        gains, cards = [], {}
        kinds = {kind: [] for kind in CARD_TYPES}
        for root in rows:
            scores = root["scores"][name]
            ref = scores[root["argmax"]]
            result = heldout_gain(scores, root["argmax"])
            if result is not None:
                gains.append(result["gain"])
            by_card, by_type = {}, {}
            for action, card in root["card_ids"].items():
                if not card:
                    continue
                pairs = [a - b for a, b in zip(scores[action], ref) if a is not None and b is not None]
                if len(pairs) < 2:
                    continue
                by_card.setdefault(card, []).append(mean_se(pairs)[0])
            for card, values in by_card.items():
                effect = mean_se(values)[0]
                cards.setdefault(card, []).append(effect)
                by_type.setdefault(catalog[card]["type"], []).append(effect)
            for kind in kinds.keys() & by_type.keys():
                kinds[kind].append(mean_se(by_type[kind])[0])
        report[name] = dict(heldout_improvement=stats(gains),
            by_card={card: dict(**catalog[card], **stats(values))
                     for card, values in sorted(cards.items()) if len(values) >= 30},
            by_type={kind: stats(values) for kind, values in kinds.items()})
    return report


def configuration(args, device, horizons, seeds):
    with args.checkpoint.open("rb") as stream:
        checkpoint = hashlib.file_digest(stream, "sha256").hexdigest()
    hardware = torch.cuda.get_device_name(device) if device.type == "cuda" else platform.machine()
    return dict(checkpoint_sha256=checkpoint, torch_version=torch.__version__, device=device.type,
                hardware=hardware, fp32=args.fp32, lanes=args.lanes, seed=args.seed, source_seeds=seeds,
                horizons=list(horizons), k=args.horizon_k)


def run(model, device, args, horizons, seeds, started, future_uniforms, continuation_actions):
    identity = configuration(args, device, horizons, seeds)
    pilot = args.horizon_pilot
    budget = PILOT_SECONDS if pilot else AUDIT_SECONDS
    deadline = started + budget
    planned = args.horizon_roots
    if args.horizon_pilot_report is not None:
        report = read_json(args.horizon_pilot_report)
        if report.get("configuration") != identity or report.get("mode") != "pilot":
            raise ValueError("Pilot checkpoint, device, K, precision, lanes, horizons and seeds must match the audit")
        planned = report["recommended_roots"]
        if not planned:
            raise ValueError("The pilot produced no usable size")
    group_size = max(1, min(64, args.lanes // (3 * args.horizon_k)))
    runner = collector(args.lanes, device, args.seed, deadline, storage_slots=group_size)
    episodes = []
    complete = True
    capture_started = time.monotonic()
    for start in range(0, len(seeds), args.lanes):
        try:
            episodes.extend(capture_episodes(model, runner, seeds[start:start+args.lanes],
                                             screen=CARD_REWARD, options_at=card_options))
        except AllocationEnded:
            complete = False
            break
    capture_seconds = time.monotonic() - capture_started
    eligible = sum(bool(ep["roots"]) for ep in episodes)
    if planned is None:
        planned = eligible
    elif complete and planned > eligible:
        raise ValueError(f"Requested {planned} roots, only {eligible} episodes have an eligible reward")
    roots = stratified_roots(episodes, min(planned, eligible), args.seed, max_per_episode=1)
    rows = []
    preview_started = time.monotonic()
    for start in range(0, len(roots), group_size):
        try:
            batch = audit_batch(runner, model, roots[start:start+group_size], horizons,
                                args.horizon_k, args.seed, future_uniforms, continuation_actions)
        except AllocationEnded:
            complete = False
            break
        atomic_json(args.output / f"card-horizons-{start:07d}.json.gz", batch)
        rows.extend(batch["roots"])
        if not batch["complete"]:
            complete = False
            break
    preview_seconds = time.monotonic() - preview_started
    usable = sum(all(all(None not in samples for samples in row["scores"][name].values())
                     for name in horizons) for row in rows)
    steps = sum(row["steps"] for row in rows)
    recommendation = 0
    if pilot and usable and preview_seconds > 0:
        remaining = max(0., (AUDIT_SECONDS - (preview_started - started) - 60) * .9)
        recommendation = min(eligible, math.floor(remaining * usable / preview_seconds))
    result = dict(mode="pilot" if pilot else "audit", complete=complete and len(rows) == planned,
                  configuration=identity, reward_units="current_reward_points", budget_seconds=budget,
                  seconds=time.monotonic() - started, capture_seconds=capture_seconds,
                  preview_seconds=preview_seconds, preview_steps=steps, planned_roots=planned,
                  evaluated_roots=len(rows), complete_roots=usable, eligible_episodes=eligible,
                  captured_episodes=len(episodes), recommended_roots=recommendation if pilot else None,
                  steps_per_second=steps / preview_seconds if preview_seconds else None,
                  wall_seconds_per_complete_root=preview_seconds / usable if usable else None,
                  summary=summarize(rows, horizons, card_catalog()))
    atomic_json(args.output / "card-horizons.json", result)
    return result
