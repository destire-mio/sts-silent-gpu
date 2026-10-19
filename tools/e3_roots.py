import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import random
import signal
import struct
import subprocess
import time

from tools.e2_roots import STAGES, digest, partition, rows
from train import OBSERVATION_VERSION


BANDS = ("k8", "k32", "k64", "k128", "k256", "k512", "act4_entry", "act3_boss", "act3_entry")


def candidates(source):
    families = defaultdict(list)
    order = ("heart_turn5", "heart_turn3", "heart_turn2", "heart_start", "act3_entry")
    for row in rows(source):
        assert partition(row["root"]["seed"]) == row["partition"]
        if row["stage"] in order:
            families[row["root"]["seed"]].append(row)
    keys = sorted(families)
    random.Random(63001).shuffle(keys)
    return [sorted(families[seed], key=lambda r: order.index(r["stage"])) for seed in keys]


def extract(lines):
    result = []
    for line in lines:
        path = line["candidates"]
        for band in BANDS:
            position = len(path) - int(band[1:]) if band.startswith("k") else line["stage_positions"][band]
            if not 0 < position < len(path):
                continue
            reached = [(value, stage) for stage, value in line["stage_positions"].items() if 0 <= value <= position]
            stage = max(reached)[1] if reached else "act1"
            result.append(dict(seed=line["seed"], candidates=path[:position], partition=line["partition"],
                band=band, distance_to_win=len(path) - position, stage=stage,
                source=dict(stage=line["stage"], attempt=line["attempt"], decisions=line["decisions"],
                            verification_decisions=line["verification_decisions"])))
    return result


def summarize(output, source, records, costs, started, reason):
    wins = [row for row in records if row["kind"] == "winning_search" and row["outcome"] == "victory"]
    roots = extract(wins)
    assert len({row["seed"] for row in wins}) == len(wins)
    for part in ("train", "dev"):
        (output / f"{part}.json").write_text(json.dumps(dict(observation_version=OBSERVATION_VERSION,
            roots=[r for r in roots if r["partition"] == part])) + "\n")
    entries = []
    audit = []
    for band in BANDS:
        for part in ("train", "dev"):
            group = sorted((r for r in roots if r["band"] == band and r["partition"] == part), key=lambda r: r["seed"])
            selected = random.Random(f"63002/{part}/{band}").sample(group, min(2, len(group)))
            audit.extend(selected)
            entries.append(dict(partition=part, band=band, roots=len(group),
                                families=len({r["seed"] for r in group})))
    document = dict(observation_version=OBSERVATION_VERSION, engine_commit=subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True).strip(), source_sha256=digest(source), roots=audit)
    (output / "audit-roots.json").write_text(json.dumps(document, indent=2) + "\n")
    report = dict(source_sha256=digest(source), stop_reason=reason, started_epoch=started,
        wall_seconds=time.time() - started, cpu_seconds=sum(c["cpu_seconds"] for c in costs),
        searched_decisions=sum(c["decisions"] for c in costs),
        families={p: len({w["seed"] for w in wins if w["partition"] == p}) for p in ("train", "dev")},
        bands=entries, unique_winning_decks=len({json.dumps(w["deck"]) for w in wins}),
        unique_winning_relic_sets=len({json.dumps(w["relics"]) for w in wins}),
        attempted_stage_counts=dict(Counter(r["stage"] for r in records if r["kind"] == "winning_search")),
        outcomes=dict(Counter(r["outcome"] for r in records if r["kind"] == "winning_search")),
        audit_roots=len(audit), audit_status="pending_mac", costs=costs,
        search_bias="First winning line per seed; later Heart roots before Act 3 entry; true-future beam with HP/card heuristics; no policy labels")
    (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


def main():
    parser = argparse.ArgumentParser(description="Bounded three-worker E3 winning search and legal-prefix inventory")
    parser.add_argument("--roots", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.time()
    groups = candidates(args.roots)
    config = dict(started_epoch=started, wall_limit_seconds=10800, cpu_limit_seconds=150 * 3600,
        workers=3, attempts_per_root=32, decisions_per_attempt=131072, source_sha256=digest(args.roots),
        binary_sha256=digest(args.binary), family_order_seed=63001,
        candidate_families=len(groups), counts=dict(Counter(f"{r['partition']}/{r['stage']}" for g in groups for r in g)))
    (args.output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    processes, streams, outputs = [], [], []
    for rank in range(3):
        prefix = args.output / f"search-{rank}"
        input_path = prefix.with_suffix(".txt")
        with input_path.open("x") as out:
            for group in groups[rank::3]:
                for row in group:
                    root = row["root"]
                    out.write(f"{row['partition']} {root['seed']} {STAGES.index(row['stage'])} {len(root['candidates'])} " +
                              " ".join(map(str, root["candidates"])) + "\n")
        out, err = prefix.with_suffix(".jsonl").open("x"), prefix.with_suffix(".stderr").open("x")
        process = subprocess.Popen([str(args.binary), "winning", str((1 << 64) - 1), "131072", "32", "10740",
                                    str(prefix.with_suffix(".journal")), str(input_path)], stdout=out, stderr=err)
        processes.append(process)
        streams.extend((out, err))
        outputs.append(prefix.with_suffix(".jsonl"))
    stopped, reason = False, "inputs_exhausted"
    try:
        while any(p.poll() is None for p in processes):
            time.sleep(10)
            records = [json.loads(line) for path in outputs for line in path.read_text().splitlines(keepends=True)
                       if line.endswith("\n")]
            won = {part: len({r["seed"] for r in records if r["kind"] == "winning_search" and
                             r["outcome"] == "victory" and r["partition"] == part}) for part in ("train", "dev")}
            failed = [p.returncode for p in processes if p.returncode not in (None, 0)]
            progress = dict(elapsed_seconds=time.time() - started, winning_families=won,
                            completed_attempts=sum(r["kind"] == "winning_search" for r in records))
            (args.output / "progress.json").write_text(json.dumps(progress) + "\n")
            print(json.dumps(progress), flush=True)
            if not stopped and (failed or time.time() - started >= 10740 or (won["train"] >= 200 and won["dev"] >= 60)):
                reason = "native_failure" if failed else "target" if won["train"] >= 200 and won["dev"] >= 60 else "wall_budget"
                for p in processes:
                    if p.poll() is None:
                        p.send_signal(signal.SIGINT)
                stopped = True
        for stream in streams:
            stream.close()
        records = [row for path in outputs for row in rows(path)]
        costs = [row for row in records if row["kind"] == "cost"]
        if any(p.returncode != 0 for p in processes):
            failures = []
            for rank, p in enumerate(processes):
                if p.returncode:
                    data = (args.output / f"search-{rank}.journal").read_bytes()
                    count, seed, length = struct.unpack_from("QQQ", data)
                    failures.append(dict(rank=rank, returncode=p.returncode, decisions=count, seed=seed,
                                         candidates=list(struct.unpack_from(f"{length}H", data, 24))))
            (args.output / "failures.json").write_text(json.dumps(failures) + "\n")
            raise RuntimeError("Native search failed; stopped all workers and preserved journals")
        assert len(costs) == 3
        summarize(args.output, args.roots, records, costs, started, reason)
    finally:
        for p in processes:
            if p.poll() is None:
                p.terminate()
        for p in processes:
            p.wait()
        for stream in streams:
            stream.close()


if __name__ == "__main__":
    main()
