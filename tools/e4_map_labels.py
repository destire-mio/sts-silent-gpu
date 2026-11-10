"""Build resumable E4 map labels or a held-out argmax regret set."""
import argparse
import base64
from collections import Counter, defaultdict
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import random
import statistics
import tempfile
import time

import torch

from tools.e4_value_residual import training_seeds
from tools.e4_value_truth import collector, load_policy, mean_se
from train.e4 import ACT, FLOOR, MAP, SCREEN
from train.e4_lookahead import MapLookaheadSearch
from train.e4_stage12 import NoSLCollector
from train.model import store_inputs

LABEL_FORMAT = 1
CHOOSE = 3
GOLD_DRAWS = 128
INITIAL_DRAWS = 4
EXTRA_DRAWS = 8


def atomic_json(path, payload):
    data = (json.dumps(payload, allow_nan=False, separators=(",", ":")) + "\n").encode()
    if path.suffix == ".gz":
        data = gzip.compress(data, mtime=0)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        except BaseException:
            stream.close()
            temporary.unlink(missing_ok=True)
            raise
    try:
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def read_json(path):
    data = path.read_bytes()
    return json.loads(gzip.decompress(data) if path.suffix == ".gz" else data)


def legal_routes(search, slots, mask):
    kinds = search.arrays["candidate_kinds"][slots]
    return [torch.where(row)[0].tolist() for row in mask[:, :kinds.shape[1]] & (kinds == CHOOSE)]


@torch.no_grad()
def capture_episodes(model, runner, seeds):
    search = runner.search
    waiting = iter(seeds)
    pending = [[] for _ in runner.seeds]
    episodes = {}
    logits = None

    def capture_logits(_module, _inputs, output):
        nonlocal logits
        logits = output[0]

    hook = model.register_forward_hook(capture_logits)
    try:
        while True:
            search.clock()
            free = [slot for slot, seed in enumerate(runner.seeds) if seed is None]
            fresh = [seed for _, seed in zip(free, waiting)]
            if fresh:
                runner.reset(free[:len(fresh)], fresh)
                for slot in free[:len(fresh)]:
                    pending[slot].clear()
            live = [slot for slot, seed in enumerate(runner.seeds) if seed is not None]
            if not live:
                break
            progress = search.progress_for(live)
            logits = None
            actions, _, _, _ = runner.choose(model, live, progress)
            healthy = [i for i, slot in enumerate(live) if slot not in search.failed]
            maps = [(j, i, live[i]) for j, i in enumerate(healthy) if progress[live[i]][SCREEN] == MAP]
            if maps:
                mask = search.last_policy_mask[[i for _, i, _ in maps]]
                options = legal_routes(search, [slot for _, _, slot in maps], mask)
                for (j, i, slot), legal in zip(maps, options):
                    if len(legal) > 1 and actions[i] in legal:
                        p = progress[slot]
                        pending[slot].append(dict(act=p[ACT], floor=p[FLOOR], progress=p,
                            prefix_length=len(runner.paths[slot]), options=legal, argmax=actions[i],
                            p_ref_logits=logits[j, legal].float().tolist()))
            _, terminal = runner.advance(live, actions, progress)
            for slot, episode in zip((s for s, done in zip(live, terminal) if done), runner.completed):
                episodes[episode["seed"]] = dict(seed=episode["seed"], path=episode["candidates"],
                    outcome=episode["outcome"], roots=pending[slot].copy())
            runner.completed.clear()
    finally:
        hook.remove()
    return [episodes[seed] for seed in seeds]


def stratified_roots(episodes, count, seed):
    buckets = defaultdict(list)
    for episode in episodes:
        for root in episode["roots"]:
            buckets[root["act"], root["floor"]].append((episode, root))
    rng = random.Random(seed)
    strata = sorted(buckets)
    rng.shuffle(strata)
    for bucket in buckets.values():
        rng.shuffle(bucket)
    selected = []
    used = Counter()
    while strata and len(selected) < count:
        next_strata = []
        for key in strata:
            bucket = buckets[key]
            while bucket:
                episode, root = bucket.pop()
                source = episode["seed"]
                if used[source] < 2:
                    used[source] += 1
                    selected.append(dict(root, seed=source, path=episode["path"][:root["prefix_length"]]))
                    break
            if bucket:
                next_strata.append(key)
            if len(selected) == count:
                break
        strata = next_strata
    if len(selected) != count:
        raise ValueError(f"Only {len(selected)} of {count} eligible roots. Supply more training seeds.")
    return selected


def replay(runner, records):
    search = runner.search
    slots = list(range(len(records)))
    runner.reset(slots, [row["seed"] for row in records])
    for tick in range(max(len(row["path"]) for row in records)):
        search.clock()
        live = [s for s in slots if tick < len(records[s]["path"])]
        progress = search.progress_for(live)
        search.mainline_progress = dict(progress)
        actions = [records[s]["path"][tick] for s in live]
        mask = search.policy_masks(live)
        if any(s in search.failed for s in live) or not bool(mask[list(range(len(live))), actions].all()):
            raise ValueError("Recorded map prefix has an invalid action")
        runner.stalled = set()
        _, terminal = runner.advance(live, actions, progress)
        if any(terminal):
            raise ValueError("Recorded map prefix ended before its root")
    progress = search.progress_for(slots)
    search.mainline_progress = dict(progress)
    options = legal_routes(search, slots, search.policy_masks(slots))
    for slot, row in enumerate(records):
        if slot in search.failed or progress[slot] != row["progress"] or options[slot] != row["options"]:
            raise ValueError(f"Map root does not replay: {row['seed']} at prefix {len(row['path'])}")
    return slots, progress


def pack_observations(search, slots):
    ids = torch.tensor(slots, device=search.device)
    arrays = search.arrays
    size = int((4 + 24 * arrays["entity_counts"][ids].long() + 7 * arrays["candidate_counts"][ids].long()).sum())
    data = torch.empty(size, dtype=torch.uint8, device=search.device)
    offsets = torch.zeros(len(slots) + 1, dtype=torch.int64, device=search.device)
    store_inputs(search.batch, arrays, ids, data, offsets)
    data, offsets = data.cpu().numpy(), offsets.tolist()
    return [base64.b64encode(data[a:b].tobytes()).decode("ascii") for a, b in zip(offsets, offsets[1:])]


def run_draws(lookahead, model, progress, samples, targets):
    plan = []
    for slot, options, seeds in targets:
        for action in options:
            plan.extend((slot, action, seed) for seed in seeds)
    capacity = len(lookahead.search.available)
    steps = 0
    for start in range(0, len(plan), capacity):
        batch = plan[start:start + capacity]
        scores, decisions = lookahead.rollout(model, batch, progress)
        steps += decisions
        for (slot, action, _), score in zip(batch, scores):
            samples[slot][action].append(score)
    return steps


def gold_result(samples, argmax):
    if any(None in row for row in samples.values()):
        return None
    half = GOLD_DRAWS // 2
    selected = max(samples, key=lambda action: statistics.mean(samples[action][:half]))
    differences = [a - b for a, b in zip(samples[selected][half:], samples[argmax][half:])]
    regret, se = mean_se(differences)
    return dict(selected=selected, regret=regret, se=se, heldout_draws=half)


def label_options(samples, argmax, retained):
    options = []
    for action, scores in samples.items():
        paired = [(a, b) for a, b in zip(scores, samples[argmax]) if a is not None and b is not None]
        returns = [a for a in scores if a is not None]
        delta, se = mean_se([a - b for a, b in paired])
        options.append(dict(action=action, retained=action in retained, attempted=len(scores), k=len(paired),
            mean=mean_se(returns)[0], delta=delta, paired_se=se,
            rollout_sd=statistics.stdev(returns) if len(returns) > 1 else None))
    return options


@torch.no_grad()
def label_shard(runner, model, records, mode, draw_seed):
    start = time.monotonic()
    slots, progress = replay(runner, records)
    samples = [dict((action, []) for action in row["options"]) for row in records]
    draws = []
    for row in records:
        rng = random.Random(f"{draw_seed}:{row['seed']}:{row['prefix_length']}")
        count = GOLD_DRAWS if mode == "gold" else INITIAL_DRAWS + EXTRA_DRAWS
        draws.append([rng.randrange(2**63) for _ in range(count)])
    lookahead = runner.search.lookahead
    count = GOLD_DRAWS if mode == "gold" else INITIAL_DRAWS
    steps = run_draws(lookahead, model, progress, samples, [
        (slot, records[slot]["options"], draws[slot][:count]) for slot in slots
    ])
    survivors = {}
    if mode == "labels":
        for slot, options in enumerate(samples):
            if any(None in scores for scores in options.values()):
                continue
            order = sorted(options, key=lambda action: statistics.mean(options[action]), reverse=True)
            survivors[slot] = order[:math.ceil(len(order) / 2)]
        steps += run_draws(lookahead, model, progress, samples, [
            (slot, list(dict.fromkeys(kept + [records[slot]["argmax"]])), draws[slot][count:])
            for slot, kept in survivors.items()
        ])
    observations = pack_observations(runner.search, slots) if mode == "labels" else [None] * len(slots)
    rows = []
    for slot, root in enumerate(records):
        scores = samples[slot]
        row = dict(root, draws=draws[slot], scores=scores)
        if mode == "gold":
            row["result"] = gold_result(scores, root["argmax"])
        else:
            row["complete"] = slot in survivors and all(None not in seq for seq in scores.values())
            row["estimates"] = label_options(scores, root["argmax"], survivors.get(slot, []))
            row["packed_observation"] = observations[slot]
        rows.append(row)
    return dict(format=LABEL_FORMAT, mode=mode, preview_steps=steps,
                seconds=time.monotonic() - start, rows=rows)


def summary(shards, mode, capture_stats):
    rows = [row for shard in shards for row in shard["rows"]]
    seconds = sum(shard["seconds"] for shard in shards)
    steps = sum(shard["preview_steps"] for shard in shards)
    result = dict(mode=mode, roots=len(rows), preview_steps=steps, labeling_seconds=seconds,
                  steps_per_root=steps / len(rows), steps_per_second=steps / seconds,
                  wall_seconds_per_root=seconds / len(rows), source=capture_stats)
    if mode == "labels":
        result["complete_roots"] = sum(row["complete"] for row in rows)
        return result
    groups = defaultdict(list)
    for row in rows:
        act, floor = row["act"], row["floor"]
        low = (floor // 4) * 4
        for key in ("all", f"act{act}", f"act{act}_floor{low}-{low+3}"):
            groups[key].append(row)
    result["argmax_regret"] = {}
    for key, group in sorted(groups.items()):
        valid = [row["result"]["regret"] for row in group if row["result"] is not None]
        mean, se = mean_se(valid)
        result["argmax_regret"][key] = dict(roots=len(group), complete=len(valid), mean=mean, se=se)
    return result


def plan_roots(args, model, device, seeds, n):
    directory = args.output
    path = directory / "roots.json.gz"
    if path.exists():
        saved = read_json(path)
        return saved["rows"], saved["source"]
    source = collector(min(args.lanes, len(seeds)), device, args.seed, float("inf"), greedy=True)
    episodes = []
    seconds = 0.
    for offset in range(0, len(seeds), args.source_shard):
        batch = seeds[offset:offset + args.source_shard]
        saved_path = directory / f"source-{offset:07d}.json.gz"
        if saved_path.exists():
            saved = read_json(saved_path)
        else:
            start = time.monotonic()
            rows = capture_episodes(model, source, batch)
            saved = dict(seconds=time.monotonic() - start, episodes=rows)
            atomic_json(saved_path, saved)
        if [row["seed"] for row in saved["episodes"]] != batch:
            raise ValueError(f"Unexpected source shard {saved_path}")
        episodes.extend(saved["episodes"])
        seconds += saved["seconds"]
    roots = stratified_roots(episodes, n, args.seed)
    stats = dict(episodes=len(episodes), eligible_roots=sum(len(ep["roots"]) for ep in episodes),
                 stalled=sum(ep["outcome"] == "stalled" for ep in episodes), seconds=seconds)
    atomic_json(path, dict(rows=roots, source=stats))
    return roots, stats


def run(args, mode, n, seeds):
    args.output.mkdir(parents=True, exist_ok=True)
    device = torch.device(args.device)
    with args.checkpoint.open("rb") as stream:
        fingerprint = hashlib.file_digest(stream, "sha256").hexdigest()
    manifest = dict(format=LABEL_FORMAT, mode=mode, roots=n, checkpoint_sha256=fingerprint,
                    source_seeds=seeds, seed=args.seed, lanes=args.lanes, preview_lanes=args.preview_lanes,
                    source_shard=args.source_shard, shard_size=args.shard_size, device=device.type, fp32=args.fp32)
    manifest_path = args.output / "manifest.json"
    if manifest_path.exists():
        if manifest != read_json(manifest_path):
            raise ValueError("Resume requires the same checkpoint, seeds, precision, mode and batch sizes")
    else:
        atomic_json(manifest_path, manifest)
    model = load_policy(args.checkpoint, device, args.fp32)
    roots, capture_stats = plan_roots(args, model, device, seeds, n)
    search = MapLookaheadSearch(args.lanes, device, args.seed, k=args.preview_lanes)
    runner = NoSLCollector(search, torch.Generator().manual_seed(args.seed))
    shards = []
    for start in range(0, n, args.shard_size):
        selected = roots[start:start + args.shard_size]
        shard_path = args.output / f"{mode}-{start:07d}.json.gz"
        if shard_path.exists():
            shard = read_json(shard_path)
            if [{key: row[key] for key in root} for root, row in zip(selected, shard["rows"])] != selected or len(shard["rows"]) != len(selected):
                raise ValueError(f"Unexpected label roots in {shard_path}")
        else:
            shard = label_shard(runner, model, selected, mode, args.seed)
            atomic_json(shard_path, shard)
        shards.append(shard)
    stats = summary(shards, mode, capture_stats)
    atomic_json(args.output / "summary.json", stats)
    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path, help="Frozen dev-112 policy")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--gold", type=int, nargs="?", const=512)
    modes.add_argument("--labels", type=int)
    parser.add_argument("--seeds", nargs="+", default=["1000000000:1000001024"])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=128)
    parser.add_argument("--preview-lanes", type=int, default=512)
    parser.add_argument("--source-shard", type=int, default=128)
    parser.add_argument("--shard-size", type=int, default=32)
    parser.add_argument("--seed", type=int, default=101)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    mode = "labels" if args.labels is not None else "gold"
    count = args.labels if mode == "labels" else 512 if args.gold is None else args.gold
    try:
        seeds = training_seeds(args.seeds)
    except ValueError as error:
        parser.error(str(error))
    if not 1 <= args.shard_size <= min(args.lanes, 512):
        parser.error("--shard-size must be in [1, min(--lanes, 512)]")
    if args.preview_lanes < 1 or args.source_shard < 1 or not 1 <= count <= 2 * len(seeds):
        parser.error("Positive batch sizes and at most two roots per source seed are required")
    torch.set_num_threads(1)
    print(json.dumps(run(args, mode, count, seeds), indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
