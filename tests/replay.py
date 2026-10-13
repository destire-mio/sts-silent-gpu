import argparse
import importlib
import json
from pathlib import Path


class Difference(AssertionError):
    pass


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


def replay(path, engine):
    with Path(path).open() as stream:
        header = json.loads(next(stream))
        if header["character"] != "THE_SILENT" or header["ascension"] != 20:
            raise ValueError("The engine supports THE_SILENT at ascension 20")
        game = engine.Game()
        if header.get("scenario") is not None:
            game.start_combat(header["seed"], header["scenario"])
        else:
            game.reset(header["seed"])
        terminal = False
        decisions = 0
        for line_number, line in enumerate(stream, 2):
            if terminal:
                raise ValueError(f"{path}:{line_number}: record follows terminal snapshot")
            record = json.loads(line)
            prefix = f"{path}:{line_number}"
            compare(record["snapshot"], game.snapshot(), f"{prefix}:$.snapshot")
            if "action" not in record:
                if game.legal_actions():
                    raise ValueError(f"{prefix}: terminal record has legal actions")
                terminal = True
                continue
            compare(record["legal"], game.legal_actions(), f"{prefix}:$.legal")
            game.step(record["action"])
            decisions += 1
        if not terminal:
            raise ValueError(f"{path}: terminal snapshot is missing")
    return decisions


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("traces", nargs="+", type=Path)
    parser.add_argument("--module", default="spire")
    args = parser.parse_args()
    engine = importlib.import_module(args.module)
    total = 0
    passed = 0
    decisions = 0
    units = {}
    for path in args.traces:
        paths = sorted(path.rglob("*.jsonl")) if path.is_dir() else [path]
        for trace in paths:
            total += 1
            with trace.open() as stream:
                header = json.loads(next(stream))
            unit = header.get("unit", (header.get("scenario") or {}).get("encounter", "full_run"))
            counts = units.setdefault(unit, [0, 0])
            counts[1] += 1
            try:
                count = replay(trace, engine)
            except (Difference, ValueError, KeyError, StopIteration) as error:
                print(f"FAIL {trace}: {error}")
            else:
                passed += 1
                counts[0] += 1
                decisions += count
                print(f"PASS {trace}: {count} decisions")
    if not total:
        raise ValueError("No JSONL traces found")
    for unit, (successes, count) in sorted(units.items()):
        print(f"unit {unit}: {successes}/{count} traces passed ({100 * successes / count:.1f}%)")
    print(f"{passed}/{total} traces passed ({100 * passed / total:.1f}%); {decisions} matched decisions")
    return int(passed != total)


if __name__ == "__main__":
    raise SystemExit(main())
