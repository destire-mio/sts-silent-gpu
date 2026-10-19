import argparse
from collections import Counter, defaultdict
import json
import os
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


def summarize(output, source, wins, costs, started, reason, prior, attempts, outcomes, stage_counts):
    roots = extract(wins)
    assert len({row["seed"] for row in wins}) == len(wins)
    (output / "winning-lines.jsonl").write_text("".join(json.dumps(line) + "\n" for line in wins))
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
    document = dict(observation_version=OBSERVATION_VERSION, status="pending_mac", round="v13-parallel-search",
        engine_commit=subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True).strip(), source_sha256=digest(source), roots=audit)
    (output / "audit-roots.json").write_text(json.dumps(document, indent=2) + "\n")
    report = dict(source_sha256=digest(source), stop_reason=reason, started_epoch=started,
        wall_seconds=prior["elapsed_seconds"] + time.time() - started,
        cpu_seconds=prior["cpu_seconds"] + sum(c["cpu_seconds"] for c in costs) + time.process_time(),
        searched_decisions=prior["decisions"] + sum(c["decisions"] for c in costs),
        retained_lines=prior["retained_lines"], replay_cost=prior,
        families={p: len({w["seed"] for w in wins if w["partition"] == p}) for p in ("train", "dev")},
        bands=entries, unique_winning_decks=len({json.dumps(w["deck"]) for w in wins}),
        unique_winning_relic_sets=len({json.dumps(w["relics"]) for w in wins}),
        attempted_stage_counts=dict(stage_counts), outcomes=dict(outcomes), completed_attempts=attempts,
        audit_roots=len(audit), audit_status="pending_mac", costs=costs,
        search_bias="First winning line per seed; later Heart roots before Act 3 entry; true-future beam with HP/card heuristics; no policy labels")
    eligible = [band for band in BANDS if all(
        next(row["families"] for row in entries if row["band"] == band and row["partition"] == part) >= 20
        for part in ("train", "dev"))]
    report["eligible_bands"] = eligible
    report["excluded_bands"] = [band for band in BANDS if band not in eligible]
    report["training_data_sufficient"] = len(eligible) >= 4
    assert report["wall_seconds"] <= 10800 and report["cpu_seconds"] <= 150 * 3600
    (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


def main():
    parser = argparse.ArgumentParser(description="Slurm CPU E3 winning search with cumulative replay and search costs")
    parser.add_argument("--roots", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--revalidated", type=Path, required=True)
    parser.add_argument("--workers", type=int, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.time()
    allocated_cpus = int(os.environ["SLURM_CPUS_PER_TASK"])
    assert 48 <= args.workers < allocated_cpus
    prior = json.loads((args.revalidated / "summary.json").read_text())
    assert prior["status"] == "replayed_and_cpu_gpu_parity_passed"
    assert prior["observation_version"] == OBSERVATION_VERSION
    wins = list(rows(args.revalidated / "winning-lines.jsonl"))
    assert len(wins) == prior["retained_lines"]
    retained = {line["seed"] for line in wins}
    groups = [group for group in candidates(args.roots) if group[0]["root"]["seed"] not in retained]
    wall_limit = min(10800 - prior["elapsed_seconds"], (150 * 3600 - prior["cpu_seconds"]) / allocated_cpus) - 60
    assert wall_limit > 0
    deadline = started + wall_limit
    config = dict(started_epoch=started, wall_limit_seconds=10800, cpu_limit_seconds=150 * 3600,
        workers=args.workers, allocated_cpus=allocated_cpus, attempts_per_root=32, decisions_per_attempt=131072,
        source_sha256=digest(args.roots), replay_summary_sha256=digest(args.revalidated / "summary.json"),
        retained_lines_sha256=digest(args.revalidated / "winning-lines.jsonl"),
        engine_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        observation_version=OBSERVATION_VERSION, search_deadline_epoch=deadline,
        cpu_limit_enforcement="Allocation CPUs times wall time is an upper bound; includes coordinator; reserve 60 seconds for finalization",
        budget_revision="main 66621552383508a7d2156a1d84154a148ebc94bf; new v13 CPU search budget including old-line replay",
        binary_sha256=digest(args.binary), family_order_seed=63001,
        candidate_families=len(groups), counts=dict(Counter(f"{r['partition']}/{r['stage']}" for g in groups for r in g)))
    (args.output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    processes, streams, readers = [], [], []
    costs, outcomes, stage_counts = [], Counter(), Counter()
    attempts = 0

    def consume():
        nonlocal attempts
        for stream in readers:
            while True:
                position = stream.tell()
                line = stream.readline()
                if not line.endswith("\n"):
                    stream.seek(position)
                    break
                row = json.loads(line)
                if row["kind"] == "cost":
                    costs.append(row)
                else:
                    assert row["kind"] == "winning_search"
                    attempts += 1
                    outcomes[row["outcome"]] += 1
                    stage_counts[row["stage"]] += 1
                    if row["outcome"] == "victory":
                        wins.append(row)

    for rank in range(args.workers):
        prefix = args.output / f"search-{rank}"
        input_path = prefix.with_suffix(".txt")
        with input_path.open("x") as out:
            for group in groups[rank::args.workers]:
                for row in group:
                    root = row["root"]
                    out.write(f"{row['partition']} {root['seed']} {STAGES.index(row['stage'])} {len(root['candidates'])} " +
                              " ".join(map(str, root["candidates"])) + "\n")
        out, err = prefix.with_suffix(".jsonl").open("x"), prefix.with_suffix(".stderr").open("x")
        process = subprocess.Popen([str(args.binary), "winning", str((1 << 64) - 1), "131072", "32", str(deadline - time.time()),
                                    str(prefix.with_suffix(".journal")), str(input_path)], stdout=out, stderr=err)
        processes.append(process)
        streams.extend((out, err))
        readers.append(prefix.with_suffix(".jsonl").open())
    stopped, reason = False, "inputs_exhausted"
    last_progress = 0.
    try:
        while any(p.poll() is None for p in processes):
            time.sleep(1)
            consume()
            won = dict(Counter(row["partition"] for row in wins))
            won = {part: won.get(part, 0) for part in ("train", "dev")}
            failed = [code for p in processes if (code := p.poll()) not in (None, 0)]
            progress = dict(elapsed_seconds=time.time() - started, winning_families=won,
                            completed_attempts=attempts, workers=args.workers,
                            cpu_seconds_upper_bound=prior["cpu_seconds"] + allocated_cpus * (time.time() - started))
            if time.time() - last_progress >= 10:
                (args.output / "progress.json").write_text(json.dumps(progress) + "\n")
                print(json.dumps(progress), flush=True)
                last_progress = time.time()
            if not stopped and (failed or time.time() >= deadline or (won["train"] >= 200 and won["dev"] >= 60)):
                reason = "native_failure" if failed else "target" if won["train"] >= 200 and won["dev"] >= 60 else "search_budget"
                for p in processes:
                    if p.poll() is None:
                        p.send_signal(signal.SIGINT)
                stopped = True
        for stream in streams:
            stream.close()
        consume()
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
        assert len(costs) == args.workers
        summarize(args.output, args.roots, wins, costs, started, reason, prior, attempts, outcomes, stage_counts)
    finally:
        for p in processes:
            if p.poll() is None:
                p.terminate()
        for p in processes:
            p.wait()
        for stream in streams:
            stream.close()
        for stream in readers:
            stream.close()


if __name__ == "__main__":
    main()
