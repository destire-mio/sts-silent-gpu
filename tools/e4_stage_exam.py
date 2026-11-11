"""Evaluate fixed held-out E2 roots through their next target boss combat."""
import argparse
from collections import Counter
import gzip
import hashlib
import json
from pathlib import Path
import random
import time

import torch

from tools.e4_map_labels import atomic_json, read_json
from tools.e4_value_truth import load_policy, mean_se
from train.e4 import ACT, BATTLE_OVER, BOSS_ROOM, COMBAT_PHASE, DEATH, ELITE_ROOM, HP, PHASE, ROOM, SCREEN, SMOKED, VICTORY
from train.e4_late_pools import stage_root_is_heldout
from train.e4_stage12 import CLSPolicy, NoSLCollector, NoSLSearch
from train.model import ModelConfig

STAGES = ("act2_boss", "act3_first_boss", "act3_second_boss", "shield_spear", "heart")
SOURCE_STAGES = {"act2_entry": ("act2_boss",), "act3_boss": ("act3_first_boss", "act3_second_boss"),
                 "act4_entry": ("shield_spear",), "heart_start": ("heart",)}
DRAWS = 4


def load_stage_policy(path, device):
    """Accept trusted full training checkpoints and plain raw/EMA snapshots."""
    saved = torch.load(path, map_location="cpu", weights_only=False, mmap=True)
    if saved.get("format") == "e4_map_distill_policy_v1":
        return load_policy(path, device)
    config = saved.get("state", {}).get("model_config")
    config = ModelConfig(**config) if config else ModelConfig(384, 6, 6, 1536)
    model = CLSPolicy(config)
    model.load_state_dict(saved.get("model", saved), strict=True)
    model.encoder_fp16 = device.type == "cuda"
    return model.to(device).eval()


def root_identity(row):
    root = row["root"]
    encoded = json.dumps([root["seed"], root["candidates"]], separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def select_e2_sources(rows, per_stage):
    has_dev = any(row["partition"] == "dev" and row["stage"] in SOURCE_STAGES for row in rows)
    selected = []
    counts = Counter()
    for row in sorted(rows, key=root_identity):
        stage = row["stage"]
        if stage not in SOURCE_STAGES:
            continue
        eligible = row["partition"] == "dev" if has_dev else (
            row["partition"] == "train" and stage_root_is_heldout(row, .2))
        if eligible and counts[stage] < per_stage:
            selected.append(row)
            counts[stage] += 1
    return selected, "dev" if has_dev else "train_root_hash_holdout_0.2"


def stage_at(progress, ledger):
    if progress[PHASE] != COMBAT_PHASE or progress[HP] <= 0:
        return None
    act, room = progress[ACT], progress[ROOM]
    if room == BOSS_ROOM:
        if act == 2:
            return "act2_boss"
        if act == 3:
            return "act3_second_boss" if ledger.reach["act3_first_boss"] else "act3_first_boss"
        if act == 4:
            return "heart"
    if act == 4 and room == ELITE_ROOM:
        return "shield_spear"
    return None


def replay_prefixes(runner, records, visit=None):
    slots = list(range(len(records)))
    runner.reset(slots, [row["seed"] for row in records])
    for tick in range(max((len(row["path"]) for row in records), default=0)):
        runner.search.clock()
        live = [slot for slot in slots if tick < len(records[slot]["path"])]
        progress = runner.search.progress_for(live)
        if visit is not None:
            for slot in live:
                visit(slot, records[slot], tick, progress[slot], runner.rewards[slot])
        runner.search.mainline_progress = dict(progress)
        choices = [records[slot]["path"][tick] for slot in live]
        # Recorded E2 actions use the engine's legal mask. Replay must not apply
        # this policy's extra noncombat/repetition exclusions to the old prefix.
        packed = runner.search.arrays["candidate_mask"][live]
        mask = ((packed[..., None].long() >> torch.arange(8, device=runner.search.device)) & 1).flatten(1).bool()
        if (any(slot in runner.search.failed for slot in live)
                or max(choices, default=0) >= mask.shape[1]
                or not bool(mask[list(range(len(live))), choices].all())):
            raise ValueError(f"E2 prefix has an illegal native action at tick {tick}")
        # E2 permits prefix actions such as a second legal map return, which
        # ordinary no-search PPO intentionally excludes. Replay the exact native
        # path and reconstruct reach ledgers without applying a new policy.
        runner.search.batch.step_slots(live, choices)
        runner.search.check(live)
        after = runner.search.progress_for(live)
        for slot, action in zip(live, choices):
            runner.paths[slot].append(action)
            runner.search.provenance[slot][1].append(action)
            runner.rewards[slot].advance(progress[slot], after[slot])
        if any(slot in runner.search.failed or after[slot][SCREEN] in (DEATH, VICTORY) for slot in live):
            raise ValueError("E2 prefix ended before the fixed root")
    runner.search.reset_seen(slots)
    runner.search.mainline_progress = runner.search.progress_for(slots)
    if visit is not None:
        for slot in slots:
            visit(slot, records[slot], len(records[slot]["path"]),
                  runner.search.mainline_progress[slot], runner.rewards[slot])
    return slots


def new_runner(lanes, device, deadline, directory, bosses_seen=0):
    search = NoSLSearch(lanes, device, 203, greedy=True, probe_slots=128, bosses_seen=bosses_seen)
    search.deadline = deadline
    search.evidence_directory = directory
    return NoSLCollector(search, torch.Generator().manual_seed(203))


@torch.no_grad()
def build_root_set(model, rows, per_stage, device, deadline, directory):
    sources, partition = select_e2_sources(rows, 256)
    runner = new_runner(1, device, deadline, directory)
    groups = {stage: [] for stage in STAGES}
    rejected = Counter()
    seen = set()
    for source in sources:
        root = source["root"]
        record = dict(seed=root["seed"], path=root["candidates"].copy())

        def visit(_slot, record, tick, progress, ledger):
            stage = stage_at(progress, ledger)
            if stage is None or (stage, record["seed"]) in seen:
                return
            seen.add((stage, record["seed"]))
            if len(groups[stage]) >= per_stage:
                return
            path = record["path"][:tick]
            derived = dict(root=dict(seed=record["seed"], candidates=path))
            if partition != "dev" and not stage_root_is_heldout(derived, .2):
                rejected["derived_combat_root_not_heldout"] += 1
                return
            identity = hashlib.sha256(json.dumps([stage, record["seed"], path]).encode()).hexdigest()
            groups[stage].append(dict(id=identity, seed=record["seed"], path=path,
                source_root=root_identity(source), stage=stage, progress=progress, bosses_seen=0))

        try:
            replay_prefixes(runner, [record], visit=visit)
        except ValueError:
            rejected["prefix_replay"] += 1
            continue
        if all(len(group) == per_stage for group in groups.values()):
            break
    return dict(format="e4_stage_exam_roots_v1", partition=partition, excluded=dict(rejected), groups=groups,
                harvest="recorded E2 prefixes only; exact derived combat-root hash held out from late training",
                missing_stages=[stage for stage in STAGES if not groups[stage]],
                boss_discovery_profile=0,
                limitation="E2 profile 0 reaches Act 4 after one Act 3 boss; no second-boss root is fabricated")


def stage_summary(rows):
    groups = {stage: [] for stage in STAGES}
    for row in rows:
        groups[row["stage"]].append(row)
    result = {}
    for stage, entries in groups.items():
        by_root = {}
        for row in entries:
            by_root.setdefault(row["root"], []).append(row)
        complete = [values for values in by_root.values()
                    if len(values) == DRAWS and all(row["complete"] for row in values)]
        wins = [sum(row["won"] for row in values) / DRAWS for values in complete]
        losses = [sum(row["hp_lost"] for row in values) / DRAWS for values in complete]
        potions = [sum(row["potions_used"] for row in values) / DRAWS for values in complete]
        mean, se = mean_se(wins)
        result[stage] = dict(roots=len(by_root), complete_roots=len(complete), draws=DRAWS,
            completed_futures=len(complete) * DRAWS, win_rate=mean, win_rate_SE=se,
            mean_hp_lost=mean_se(losses)[0], mean_potions_used=mean_se(potions)[0])
    return result


def paired_stage_summary(candidate, baseline):
    reference = {(row["root"], row["draw"]): row for row in baseline}
    by_stage = {stage: {} for stage in STAGES}
    for row in candidate:
        other = reference[row["root"], row["draw"]]
        if row["complete"] and other["complete"]:
            by_stage[row["stage"]].setdefault(row["root"], []).append(
                (int(row["won"]) - int(other["won"]), row["hp_lost"] - other["hp_lost"]))
    result = {}
    for stage, groups in by_stage.items():
        paired = [values for values in groups.values() if len(values) == DRAWS]
        mean, se = mean_se([sum(row[0] for row in values) / DRAWS for values in paired])
        hp, hp_se = mean_se([sum(row[1] for row in values) / DRAWS for values in paired])
        result[stage] = dict(paired_roots=len(paired), paired_draws=len(paired) * DRAWS,
                             win_rate_delta=mean, paired_SE=se, hp_lost_delta=hp, hp_lost_paired_SE=hp_se)
    return result


@torch.no_grad()
def exam_chunk(model, records, device, deadline, directory):
    profiles = {row.get("bosses_seen", 0) for row in records}
    if len(profiles) != 1:
        raise ValueError("A stage exam chunk must share its recorded boss discovery profile")
    runner = new_runner(len(records) * DRAWS, device, deadline, directory, profiles.pop())
    expanded = [row for row in records for _ in range(DRAWS)]
    slots = replay_prefixes(runner, expanded)
    progress = runner.search.progress_for(slots)
    if any(progress[slot] != row["progress"] for slot, row in enumerate(expanded)):
        raise ValueError("Fixed stage root changed while replaying")
    futures = [random.Random(f"203:{row['id']}:{draw}").randrange(2**63)
               for row in records for draw in range(DRAWS)]
    runner.search.batch.determinize_slots(slots, futures, True)
    for slot, key in zip(slots, runner.search.loop_keys(slots)):
        runner.search.seen[slot] = {key}
        runner.search.mask_cache.pop(slot, None)
        runner.search.repeat_blocked.pop(slot, None)
    initial_hp = {slot: progress[slot][HP] for slot in slots}
    potions = dict.fromkeys(slots, 0)
    result = {}
    live = slots
    while live:
        runner.search.clock()
        before = runner.search.progress_for(live)
        actions, _, _, _ = runner.choose(model, live, before)
        for slot, action in zip(live, actions):
            potions[slot] += int(runner.search.arrays["candidate_kinds"][slot, action]) == 4
        previous = len(runner.completed)
        _, terminal = runner.advance(live, actions, before)
        after = runner.search.progress_for(live)
        ended = dict(zip((slot for slot, done in zip(live, terminal) if done), runner.completed[previous:]))
        remaining = []
        for slot in live:
            p = after[slot]
            left = before[slot][PHASE] == COMBAT_PHASE and p[PHASE] != COMBAT_PHASE
            complete = left or slot in ended
            if not complete:
                remaining.append(slot)
                continue
            won = left and bool(p[BATTLE_OVER]) and not p[SMOKED] and p[HP] > 0
            row = expanded[slot]
            valid = slot not in runner.search.failed and ended.get(slot, {}).get("outcome") != "stalled"
            result[slot] = dict(root=row["id"], source_seed=row["seed"], stage=row["stage"],
                draw=slot % DRAWS, determinize_seed=futures[slot], complete=valid, won=bool(won),
                hp_lost=initial_hp[slot] - p[HP], potions_used=potions[slot])
        live = remaining
    return [result[slot] for slot in slots]


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--roots", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--per-stage", type=int, default=256)
    parser.add_argument("--lanes", type=int, default=64)
    parser.add_argument("--device", default="cuda")
    parser.add_argument("--max-seconds", type=float, default=21600.)
    args = parser.parse_args(argv)
    if not 1 <= args.per_stage <= 256 or args.lanes < DRAWS:
        parser.error("per-stage must be 1..256 and lanes at least 4")
    args.output.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(1)
    device = torch.device(args.device)
    started = time.monotonic()
    deadline = started + args.max_seconds
    root_file = args.output / "roots.json.gz"
    raw = args.roots.read_bytes()
    input_sha256 = hashlib.sha256(raw).hexdigest()
    if root_file.exists():
        roots = read_json(root_file)
        if roots["input_sha256"] != input_sha256 or roots["per_stage"] != args.per_stage:
            raise ValueError("Stage root input or per-stage count changed on resume")
    else:
        decoded = gzip.decompress(raw) if args.roots.suffix == ".gz" else raw
        first = json.loads(decoded.splitlines()[0])
        if first.get("format") == "e4_stage_exam_roots_v1":
            roots = json.loads(decoded)
        else:
            rows = [json.loads(line) for line in decoded.splitlines() if line]
            roots = build_root_set(None, rows, args.per_stage, device, deadline, args.output)
            roots["source_sha256"] = hashlib.sha256(raw).hexdigest()
        roots.update(input_sha256=input_sha256, per_stage=args.per_stage)
        atomic_json(root_file, roots)
    records = [row for stage in STAGES for row in roots["groups"][stage]]
    if not records:
        raise ValueError("No valid held-out stage roots were recovered")
    reports = {}
    rows_by_mode = {}
    for label, checkpoint in (("candidate", args.checkpoint), ("baseline", args.baseline)):
        if checkpoint is None:
            continue
        model = load_stage_policy(checkpoint, device)
        with checkpoint.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        model_rows = []
        mode_started = time.monotonic()
        chunk_size = max(1, args.lanes // DRAWS)
        chunks = []
        for profile in sorted({row.get("bosses_seen", 0) for row in records}):
            group = [row for row in records if row.get("bosses_seen", 0) == profile]
            chunks.extend(group[start:start + chunk_size] for start in range(0, len(group), chunk_size))
        start = 0
        for selected in chunks:
            file = args.output / f"{label}-{start:05d}.json.gz"
            root_ids = [row["id"] for row in selected]
            if file.exists():
                chunk = read_json(file)
                if chunk["checkpoint_sha256"] != digest or chunk["root_ids"] != root_ids:
                    raise ValueError("Stage exam checkpoint or roots changed on resume")
            else:
                chunk = dict(checkpoint_sha256=digest, root_ids=root_ids,
                    rows=exam_chunk(model, selected, device, deadline, args.output))
                atomic_json(file, chunk)
            model_rows.extend(chunk["rows"])
            start += len(selected)
        reports[label] = dict(checkpoint=str(checkpoint), checkpoint_sha256=digest,
                             seconds=time.monotonic() - mode_started, stages=stage_summary(model_rows))
        rows_by_mode[label] = model_rows
        del model
    report = dict(root_set=str(root_file), partition=roots["partition"], root_exclusions=roots["excluded"],
                  models=reports, seconds=time.monotonic() - started, seed_unit="root means over four paired futures",
                  missing_stages=roots.get("missing_stages", []), limitation=roots.get("limitation"),
                  boss_discovery_profiles=sorted({row.get("bosses_seen", 0) for row in records}))
    if args.baseline:
        report["paired"] = paired_stage_summary(rows_by_mode["candidate"], rows_by_mode["baseline"])
    atomic_json(args.output / "report.json", report)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

