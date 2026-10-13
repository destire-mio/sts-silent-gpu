"""Record the standard card/potion scenarios and derive their inventory status."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import sys

import replay

ROOT = Path(__file__).resolve().parents[1]
SEEDS = (101, 102, 103)


def content_cases():
    inventory = json.loads((ROOT / "inventory/units.json").read_text())
    units = {Path(u["java_path"]).stem: u for u in inventory["units"]
             if u["kind"] in ("card", "potion") and u["stage"] == "standard"}
    cases = {}
    for path in sorted((ROOT / "tests/scenarios").glob("*.json")):
        name = path.stem.split("--")[-1]
        owners = [unit for unit in units if name == unit or name.startswith(unit + "-")]
        if owners:
            cases[path.stem] = (path, units[max(owners, key=len)])
    for unit in units:
        assert unit in cases, f"Missing primary scenario: {unit}"
    return units, cases


def missing_content(scenario):
    missing = []
    for kind, values in (("card", [c["id"] for c in scenario["deck"]]),
                         ("relic", scenario["relics"]),
                         ("potion", [p for p in scenario["potions"] if p is not None]),
                         ("encounter", [scenario["encounter"]])):
        catalog = (ROOT / "core" / (kind + "s") / "catalog.def").read_text()
        known = set(re.findall(r'SPIRE_UNIT\(\w+, "([^"]+)"', catalog))
        missing.extend({"kind": kind, "id": value} for value in values if value not in known)
    return missing


def record_content(output, cases):
    sys.path.insert(0, str(ROOT / "oracle"))
    from record import run_jobs
    jobs = [dict(seed=seed, scenario=json.loads(path.read_text()),
                 output=str(output.resolve() / name / f"{seed}.jsonl"))
            for name, (path, _) in cases.items() for seed in SEEDS]
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(run_jobs, [jobs[::2], jobs[1::2]]))


def replay_trace(path, engine):
    read_fd, write_fd = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.close(read_fd)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        try:
            result = replay.replay(path, engine)
            row = dict(status="matched" if result.passed else "different",
                       decisions=result.decisions,
                       scopes={name: dict(matched=scope.matched_decisions,
                                         terminal=scope.terminal_matches,
                                         first_difference=scope.differences[0] if scope.differences else result.stopped)
                               for name, scope in result.scopes.items()})
        except (replay.Difference, ValueError, KeyError, StopIteration) as error:
            row = dict(status="error", error=str(error))
        os.write(write_fd, json.dumps(row).encode())
        os.close(write_fd)
        os._exit(0)
    os.close(write_fd)
    with os.fdopen(read_fd) as stream:
        encoded = stream.read()
    _, status = os.waitpid(pid, 0)
    if status:
        return dict(status="fault", error=f"Replay process exited with status {status}")
    return json.loads(encoded)


def source_digest():
    digest = hashlib.sha256()
    files = [p for folder in ("core", "host", "cuda", "oracle/java")
             for p in (ROOT / folder).rglob("*")
             if p.suffix in (".hpp", ".cpp", ".cuh", ".cu", ".def", ".py", ".java")]
    files += [ROOT / "inventory" / name for name in
              ("units.json", "snapshot-fields.json", "runtime-patches.json")]
    for path in sorted(files):
        digest.update(str(path.relative_to(ROOT)).encode() + b"\0" + path.read_bytes())
    return digest.hexdigest()


def replay_content(output, units, cases):
    import spire
    document = dict(schema=1, source_sha256=source_digest(), seeds=list(SEEDS),
                    scope="Both scopes and terminal snapshots; blocked supplements are not passes",
                    units={}, scenarios={})
    failed = False
    for name, (scenario_path, unit) in cases.items():
        scenario = json.loads(scenario_path.read_text())
        missing = missing_content(scenario)
        rows = []
        for seed in SEEDS:
            path = output / name / f"{seed}.jsonl"
            with path.open() as stream:
                header = json.loads(next(stream))
            assert header["seed"] == seed and header["scenario"] == scenario, path
            row = replay_trace(path, spire)
            if row["status"] == "error" and row["error"] in ["Unknown ID: " + m["id"] for m in missing]:
                row["status"] = "blocked"
            for scope in row.get("scopes", {}).values():
                if scope["first_difference"]:
                    scope["first_difference"] = scope["first_difference"].replace(str(path), f"{name}/{seed}.jsonl")
            row.update(seed=seed, trace_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
            rows.append(row)
            print(name, seed, row["status"], row.get("error", ""), flush=True)
            if row["status"] not in ("matched", "blocked"):
                failed = True
                for scope in row.get("scopes", {}).values():
                    if scope["first_difference"]:
                        print("  " + scope["first_difference"], flush=True)
        document["scenarios"][name] = dict(unit=unit["key"],
            scenario_sha256=hashlib.sha256(scenario_path.read_bytes()).hexdigest(),
            waiting_for=missing, results=rows)
    for name, unit in sorted(units.items()):
        selected = {key: value for key, value in document["scenarios"].items() if value["unit"] == unit["key"]}
        primary = selected[name]["results"]
        runnable = [row for value in selected.values() for row in value["results"] if row["status"] != "blocked"]
        matched = all(row["status"] == "matched" for row in primary + runnable)
        failed |= not matched
        document["units"][unit["key"]] = dict(status="matched" if matched else "different",
            scenarios=list(selected), matched_traces=sum(row["status"] == "matched" for row in runnable),
            blocked_traces=sum(row["status"] == "blocked" for value in selected.values() for row in value["results"]))
    rows = [row for value in document["scenarios"].values() for row in value["results"]]
    document["totals"] = dict(
        units={kind: dict(matched=sum(value["status"] == "matched" for key, value in document["units"].items() if key.startswith(kind + ":")),
                          total=sum(key.startswith(kind + ":") for key in document["units"])) for kind in ("card", "potion")},
        traces={status: sum(row["status"] == status for row in rows) for status in ("matched", "blocked", "different", "error", "fault")},
        scopes={scope: dict(matched=sum(row.get("scopes", {}).get(scope, {}).get("matched", 0) for row in rows),
                            decisions=sum(row.get("decisions", 0) for row in rows)) for scope in ("combat", "run")})
    (ROOT / "inventory/status.json").write_text(json.dumps(document, indent=2) + "\n")
    print(json.dumps(document["totals"], indent=2))
    return int(failed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("record", "replay"))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    units, cases = content_cases()
    if args.command == "record":
        record_content(args.output, cases)
    else:
        raise SystemExit(replay_content(args.output, units, cases))
