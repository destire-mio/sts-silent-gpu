"""Record content scenarios and derive inventory status from strict two-scope replays."""
import argparse
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


def catalogs():
    return {kind: re.findall(r'SPIRE_UNIT\((\w+), "([^\"]+)", "([^\"]+)", (\w+)\)',
                            (ROOT / "core" / (kind + "s") / "catalog.def").read_text())
            for kind in ("card", "potion", "relic", "monster", "encounter", "power", "action", "event")}


def content_cases():
    inventory = json.loads((ROOT / "inventory/units.json").read_text())["units"]
    known = catalogs()
    units = {}
    for kind, entries in known.items():
        for enum, game_id, header, _ in entries:
            matches = [u for u in inventory if u["kind"] == kind and
                       (u["java_class"].removeprefix("com.megacrit.cardcrawl.") == game_id
                        if kind == "action" else u["game_id"] == game_id)]
            if not matches and kind == "potion" and enum == "Empty":
                continue
            if not matches:
                assert kind in ("action", "event"), (kind, game_id)
                java_class = game_id if kind == "action" else re.search(r'return \{"([^"]+)"', (ROOT / header).read_text())[1]
                matches = [dict(key=kind + ":" + java_class, kind=kind, game_id=game_id,
                                java_class="com.megacrit.cardcrawl." + java_class,
                                java_path=java_class.replace(".", "/") + ".java", facts={}, stage="internal")]
            unit = dict(matches[0], header=header, enum=enum)
            units[unit["key"]] = unit
    cases = {}
    for path in sorted((ROOT / "tests/scenarios").glob("*.json")):
        name = path.stem.split("--")[-1]
        setup = json.loads(path.read_text())
        if "start" in setup:
            continue
        owners = []
        for key, unit in units.items():
            if unit["kind"] == "encounter":
                owns = setup["encounter"] == unit["game_id"]
            elif unit["kind"] == "event":
                owns = False
            else:
                names = {Path(unit["java_path"]).stem, unit["enum"]}
                owns = any(name == n or name.startswith(n + "-") for n in names)
            if owns:
                owners.append(key)
        cases[path.stem] = (path, owners)
    for key, unit in units.items():
        if unit["kind"] in ("card", "potion", "relic", "monster"):
            assert any(key in owners for _, owners in cases.values()), f"Missing scenario: {key}"
    return units, cases


RULE_CALLERS = {
    "onEnterRoom": ("core/engine.hpp", "relic_onEnterRoom("),
    "justEnteredRoom": ("core/engine.hpp", "relic_justEnteredRoom("),
    "onEnterRestRoom": ("core/campfire.hpp", "relic_onEnterRestRoom("),
    "onChestOpen": ("core/treasure.hpp", "relic_onChestOpen("),
    "onChestOpenAfter": ("core/treasure.hpp", "relic_onChestOpenAfter("),
    "addCampfireOption": ("core/campfire.hpp", "relic_addCampfireOption("),
    "canUseCampfireOption": ("core/campfire.hpp", "relic_canUseCampfireOption("),
    "onRest": ("core/campfire.hpp", "relic_onRest("),
    "onSmith": ("core/campfire.hpp", "relic_onSmith("),
    "changeNumberOfCardsInReward": ("core/rewards.hpp", "relic_changeNumberOfCardsInReward("),
    "changeRareCardRewardChance": ("core/lifecycle.hpp", "relic_changeRareCardRewardChance("),
    "changeUncommonCardRewardChance": ("core/lifecycle.hpp", "relic_changeUncommonCardRewardChance("),
    "canSpawn": ("core/lifecycle.hpp", "relic_can_spawn_id("),
    "deathReact": ("core/monsters/bandit_bear.hpp", "monster_deathReact("),
    "boss room flag": ("core/engine.hpp", "g.room_kind = room;"),
    "shop_multiplier": ("core/shop.hpp", ".shop_multiplier"),
    "purge_cost": ("core/shop.hpp", ".purge_cost"),
    "rest_heal_bonus": ("core/campfire.hpp", ".rest_heal_bonus"),
    "bowl_hp": ("core/engine.hpp", ".bowl_hp"),
    "event_chest_interval": ("core/event_room.hpp", ".event_chest_interval"),
    "crosses_map_paths": ("core/engine.hpp", ".crosses_map_paths"),
    "stacks_on_obtain": ("core/lifecycle.hpp", ".stacks_on_obtain"),
}


def rule_consumers(unit):
    source = (ROOT / unit["header"]).read_text()
    hooks = set(unit["facts"].get("hooks", [])) & RULE_CALLERS.keys()
    if unit["kind"] != "relic":
        hooks.discard("canSpawn")
    if "RoomKind::Boss" in source:
        hooks.add("boss room flag")
    for field in ("shop_multiplier", "purge_cost", "rest_heal_bonus", "bowl_hp",
                  "event_chest_interval", "crosses_map_paths", "stacks_on_obtain"):
        if "." + field in source:
            hooks.add(field)
    result = {}
    for hook in sorted(hooks):
        path, expression = RULE_CALLERS[hook]
        lines = (ROOT / path).read_text().splitlines()
        matches = [index + 1 for index, line in enumerate(lines) if expression in line]
        assert matches, (hook, path, expression)
        result[hook] = [f"{path}:{line}" for line in matches]
    if unit["kind"] == "event" and "canSpawn(" in source:
        path = "core/event_room.hpp"
        result["canSpawn"] = [f"{path}:{index + 1}" for index, line in
                              enumerate((ROOT / path).read_text().splitlines()) if "event_canSpawn(" in line]
    return result


def waiting_rules(unit):
    source = (ROOT / unit["header"]).read_text()
    waiting = set(re.findall(r'SPIRE_WAITING_RULE\("([^\"]+)"\)', source))
    return sorted(waiting)


def not_applicable(unit):
    source = (ROOT / unit["header"]).read_text()
    return sorted(re.findall(r'SPIRE_NOT_APPLICABLE\("([^\"]+)"\)', source))


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
    run_jobs(jobs)


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


def trace_evidence(path, units):
    classes = {u["java_class"].removeprefix("com.megacrit.cardcrawl."): key for key, u in units.items()}
    known_relics = {u["game_id"] for u in units.values() if u["kind"] == "relic"}
    observed, missing, decisions = set(), [], 0
    with replay.open_trace(path) as stream:
        header = json.loads(next(stream))
        for line in stream:
            row = json.loads(line)
            decisions += "action" in row
            for obj in row["snapshot"]["objects"]:
                if obj["class"] in classes:
                    observed.add(classes[obj["class"]])
                if obj["class"].startswith("relics."):
                    relic = obj["fields"]["AbstractRelic.relicId"]
                    dependency = dict(kind="relic", id=relic)
                    if relic not in known_relics and dependency not in missing:
                        missing.append(dependency)
        assert "terminal" in row, path
    return header, observed, missing, decisions


def replay_content(output, units, cases, run_paths=()):
    import spire
    document = dict(schema=2, source_sha256=source_digest(), seeds=list(SEEDS),
                    scope="Both scopes and terminal snapshots; missing units and rules-only hooks are separate",
                    units={}, scenarios={})
    inventory = json.loads((ROOT / "inventory/units.json").read_text())["units"]
    document["not_implemented"] = [u["key"] for u in inventory if u["kind"] in catalogs()
                                   and u["stage"] == "standard" and u["key"] not in units]
    observations = {key: [] for key in units}
    failed = False
    for name, (scenario_path, owners) in cases.items():
        scenario = json.loads(scenario_path.read_text())
        missing = missing_content(scenario)
        rules = [dict(kind="rule", id=name.split("--")[1].removeprefix("rule-"))] if name.startswith("blocked--rule-") else []
        rows = []
        for seed in SEEDS:
            path = output / name / f"{seed}.jsonl"
            header, observed, generated_missing, decisions = trace_evidence(path, units)
            assert header["seed"] == seed and header["scenario"] == scenario, path
            dependencies = missing + generated_missing
            row = dict(status="blocked", decisions=decisions, waiting_for=dependencies) if dependencies else replay_trace(path, spire)
            for scope in row.get("scopes", {}).values():
                if scope["first_difference"]:
                    scope["first_difference"] = scope["first_difference"].replace(str(path), f"{name}/{seed}.jsonl")
            row.update(seed=seed, trace_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
            rows.append(row)
            if row["status"] == "matched":
                for key in observed:
                    observations[key].append(name)
                    if units[key]["kind"] == "event" and key not in owners:
                        owners.append(key)
            print(name, seed, row["status"], row.get("error", ""), flush=True)
            if row["status"] not in ("matched", "blocked"):
                failed = True
                for scope in row.get("scopes", {}).values():
                    if scope["first_difference"]:
                        print("  " + scope["first_difference"], flush=True)
        document["scenarios"][name] = dict(units=owners,
            scenario_sha256=hashlib.sha256(scenario_path.read_bytes()).hexdigest(),
            waiting_for=missing + rules, results=rows)
    for path in run_paths:
        header, observed, missing, decisions = trace_evidence(path, units)
        assert header["scenario"] is None or "start" in header["scenario"], path
        assert not missing, (path, missing)
        name = "run:" + path.name.removesuffix(".gz").removesuffix(".jsonl")
        assert name not in document["scenarios"], name
        row = replay_trace(path, spire)
        row.update(seed=header["seed"], trace_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        document["scenarios"][name] = dict(units=sorted(observed), waiting_for=[], results=[row])
        for scope in row.get("scopes", {}).values():
            if scope["first_difference"]:
                scope["first_difference"] = scope["first_difference"].replace(str(path), name)
        if row["status"] == "matched":
            for key in observed:
                observations[key].append(name)
        else:
            failed = True
        print(name, json.dumps(row), flush=True)
    for key, unit in sorted(units.items()):
        selected = {name: value for name, value in document["scenarios"].items() if key in value["units"]}
        # An encounter's acceptance excludes foreign-content supplements; its ordinary scenarios must match.
        eligible = [row for value in selected.values() if not value["waiting_for"] for row in value["results"]]
        bad = [row for row in eligible if row["status"] not in ("matched", "blocked")]
        blocked = [row for value in selected.values() for row in value["results"] if row["status"] == "blocked"]
        primary = Path(unit["java_path"]).stem
        primary_rows = [row for name, value in selected.items() if name.split("--")[-1] == primary for row in value["results"]]
        status = "different" if bad else "blocked" if any(r["status"] == "blocked" for r in primary_rows) else "matched" if eligible or observations[key] else "unobserved"
        document["units"][key] = dict(status=status, scenarios=list(selected),
            observed_in=sorted(set(observations[key])), waiting_for_rules=waiting_rules(unit),
            not_applicable=not_applicable(unit),
            rule_callers=rule_consumers(unit),
            matched_traces=sum(row["status"] == "matched" for row in eligible), blocked_traces=len(blocked))
    rows = [row for value in document["scenarios"].values() for row in value["results"]]
    document["totals"] = dict(
        units={kind: dict(**{status: sum(value["status"] == status for key, value in document["units"].items() if key.startswith(kind + ":"))
                            for status in ("matched", "blocked", "different", "unobserved")},
                          waiting_for_rules=sum(bool(value["waiting_for_rules"]) for key, value in document["units"].items() if key.startswith(kind + ":")),
                          total=sum(key.startswith(kind + ":") for key in document["units"]),
                          not_implemented=sum(key.startswith(kind + ":") for key in document["not_implemented"])) for kind in catalogs()},
        traces={status: sum(row["status"] == status for row in rows) for status in ("matched", "blocked", "different", "error", "fault")},
        scopes={scope: dict(matched=sum(row.get("scopes", {}).get(scope, {}).get("matched", 0) for row in rows),
                            decisions=sum(row.get("decisions", 0) for row in rows if row["status"] != "blocked")) for scope in ("combat", "run")},
        recorded_decisions=sum(row.get("decisions", 0) for row in rows))
    (ROOT / "inventory/status.json").write_text(json.dumps(document, indent=2) + "\n")
    print(json.dumps(document["totals"], indent=2))
    return int(failed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("record", "replay"))
    parser.add_argument("output", type=Path)
    parser.add_argument("--runs", nargs="*", type=Path, default=[])
    args = parser.parse_args()
    units, cases = content_cases()
    if args.command == "record":
        record_content(args.output, cases)
    else:
        raise SystemExit(replay_content(args.output, units, cases, replay.trace_paths(args.runs)))
