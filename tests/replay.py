import argparse
import gzip
import importlib
import json
from pathlib import Path


GAME_SHA256 = "dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a"


class Difference(AssertionError):
    matched_decisions = 0


def compare(expected, actual, path="$"):
    if type(expected) is not type(actual):
        raise Difference(f"{path}: expected {expected!r} ({type(expected).__name__}), got {actual!r} ({type(actual).__name__})")
    if isinstance(expected, dict):
        for key in expected:
            if key not in actual:
                raise Difference(f"{path}.{key}: expected {expected[key]!r}, got <missing>")
            compare(expected[key], actual[key], f"{path}.{key}")
        for key in actual:
            if key not in expected:
                raise Difference(f"{path}.{key}: expected <missing>, got {actual[key]!r}")
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            raise Difference(f"{path}.length: expected {len(expected)}, got {len(actual)}")
        for index, (wanted, observed) in enumerate(zip(expected, actual)):
            compare(wanted, observed, f"{path}[{index}]")
    elif expected != actual:
        raise Difference(f"{path}: expected {expected!r}, got {actual!r}")


def open_trace(path):
    path = Path(path)
    return gzip.open(path, "rt", encoding="utf-8") if path.suffix == ".gz" else path.open(encoding="utf-8")


def terminal_reason(snapshot, scenario):
    player = snapshot["objects"][snapshot["player"]["ref"]]["fields"]
    room = snapshot["objects"][snapshot["room"]["ref"]]["fields"]
    dead = player["AbstractCreature.isDead"]
    finished = dead or snapshot["screen"]["type"] in ("DEATH", "VICTORY")
    finished |= scenario and room["AbstractRoom.isBattleOver"]
    if not finished:
        return None
    return "death" if dead else "combat_end" if scenario else "victory"


def replay(path, engine):
    decisions = 0
    with open_trace(path) as stream:
        header = json.loads(next(stream))
        compare(1, header["schema"], f"{path}:1:$.schema")
        compare(GAME_SHA256, header["game_sha256"], f"{path}:1:$.game_sha256")
        compare("THE_SILENT", header["character"], f"{path}:1:$.character")
        compare(20, header["ascension"], f"{path}:1:$.ascension")
        game = engine.Game()
        scenario = header["scenario"] is not None
        if scenario:
            game.start_combat(header["seed"], header["scenario"])
        else:
            game.reset(header["seed"])
        terminal = False
        for line_number, line in enumerate(stream, 2):
            if terminal:
                raise ValueError(f"{path}:{line_number}: record follows terminal snapshot")
            record = json.loads(line)
            prefix = f"{path}:{line_number}:$"
            try:
                snapshot = game.snapshot()
                compare(record["snapshot"], snapshot, f"{prefix}.snapshot")
                if "terminal" in record:
                    compare(record["terminal"], terminal_reason(snapshot, scenario), f"{prefix}.terminal")
                    terminal = True
                    continue
                compare(record["legal"], game.legal_actions(), f"{prefix}.legal")
            except Difference as error:
                error.matched_decisions = decisions
                raise
            game.step(record["action"])
            decisions += 1
        if not terminal:
            raise ValueError(f"{path}: terminal snapshot is missing")
    return decisions


def trace_paths(paths):
    for path in paths:
        if not path.is_dir():
            yield path
            continue
        candidates = sorted((*path.rglob("*.jsonl"), *path.rglob("*.jsonl.gz")))
        for trace in candidates:
            with open_trace(trace) as stream:
                header = json.loads(next(stream))
            if header["scenario"] is not None:
                yield trace


def main(argv=None):
    parser = argparse.ArgumentParser(description="Replay original-game combat traces with exact comparisons")
    parser.add_argument("traces", nargs="*", type=Path,
                        default=[Path(__file__).parent / "traces"],
                        help="JSONL/gzip files or combat-trace directories; full runs require an explicit file")
    parser.add_argument("--module", default="spire")
    args = parser.parse_args(argv)
    engine = importlib.import_module(args.module)
    total = 0
    passed = 0
    decisions = 0
    for trace in trace_paths(args.traces):
        total += 1
        try:
            count = replay(trace, engine)
        except Difference as error:
            decisions += error.matched_decisions
            print(f"FAIL {trace}: {error}")
        except (ValueError, KeyError, StopIteration) as error:
            print(f"FAIL {trace}: {error}")
        else:
            passed += 1
            decisions += count
            print(f"PASS {trace}: {count} decisions")
    if not total:
        raise ValueError("No combat JSONL traces found")
    print(f"{passed}/{total} traces passed; {decisions} matched decisions")
    return int(passed != total)


if __name__ == "__main__":
    raise SystemExit(main())
