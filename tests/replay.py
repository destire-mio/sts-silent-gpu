import argparse
import gzip
import importlib
import json
from dataclasses import dataclass, field
from pathlib import Path


GAME_SHA256 = "dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a"


class Difference(AssertionError):
    pass


def compare(expected, actual, path="$", refs=None):
    if type(expected) is not type(actual):
        raise Difference(f"{path}: expected {expected!r} ({type(expected).__name__}), got {actual!r} ({type(actual).__name__})")
    if isinstance(expected, dict):
        if refs is not None and (set(expected) == {"ref"} or set(actual) == {"ref"}):
            if set(expected) != {"ref"} or set(actual) != {"ref"}:
                raise Difference(f"{path}: expected {expected!r}, got {actual!r}")
            refs.compare(expected["ref"], actual["ref"], path)
            return
        for key in expected:
            if key not in actual:
                raise Difference(f"{path}.{key}: expected {expected[key]!r}, got <missing>")
            if refs is not None and key == "AbstractCard.uuid":
                refs.compare_uuid(expected[key], actual[key], f"{path}.{key}")
            else:
                compare(expected[key], actual[key], f"{path}.{key}", refs)
        for key in actual:
            if key not in expected:
                raise Difference(f"{path}.{key}: expected <missing>, got {actual[key]!r}")
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            raise Difference(f"{path}.length: expected {len(expected)}, got {len(actual)}")
        for index, (wanted, observed) in enumerate(zip(expected, actual)):
            compare(wanted, observed, f"{path}[{index}]", refs)
    elif expected != actual:
        raise Difference(f"{path}: expected {expected!r}, got {actual!r}")


class GraphComparison:
    def __init__(self, expected, actual, scope, identities=None):
        self.expected = expected["objects"]
        self.actual = actual["objects"]
        self.scope = scope
        self.forward, self.reverse, self.uuid_forward, self.uuid_reverse = identities or ({}, {}, {}, {})
        self.visited = set()

    def bind(self, expected, actual, forward, reverse, path):
        if type(expected) is not int or type(actual) is not int:
            raise Difference(f"{path}: expected integer identity {expected!r}, got {actual!r}")
        if expected in forward:
            if forward[expected] != actual:
                raise Difference(f"{path}: expected identity of {expected!r} mapped to {forward[expected]!r}, got {actual!r}")
            return False
        if actual in reverse:
            raise Difference(f"{path}: expected distinct identity {expected!r}, got shared identity {actual!r} already mapped from {reverse[actual]!r}")
        forward[expected] = actual
        reverse[actual] = expected
        return True

    def compare_uuid(self, expected, actual, path):
        self.bind(expected, actual, self.uuid_forward, self.uuid_reverse, path)

    def compare(self, expected, actual, path):
        self.bind(expected, actual, self.forward, self.reverse, path)
        if expected in self.visited:
            return
        self.visited.add(expected)
        wanted = self.expected[expected]
        observed = self.actual[actual]
        if self.scope == "run":
            wanted = self.room_fields(wanted)
            observed = self.room_fields(observed)
        compare(wanted, observed, path, self)

    @staticmethod
    def room_fields(value):
        return {**value, "fields": {key: field for key, field in value["fields"].items()
                                    if key != "AbstractRoom.monsters"}}


COMBAT_RNG = {"aiRng", "shuffleRng", "cardRandomRng", "monsterHpRng", "miscRng", "unseeded"}
COMBAT_ROOTS = {"player", "manager", "combat"}
RUN_SCREEN_FIELDS = {"room_dialog", "image_dialog"}
REWARD_SCREENS = {"COMBAT_REWARD", "BOSS_REWARD", "SHOP"}


def scope_roots(snapshot, scope):
    screen = snapshot["screen"]
    state_in_run = screen["type"] in REWARD_SCREENS
    combat_screen = {key: value for key, value in screen.items()
                     if key not in RUN_SCREEN_FIELDS and not (key == "state" and state_in_run)}
    run_screen = {key: value for key, value in screen.items()
                  if key in RUN_SCREEN_FIELDS or (key == "state" and state_in_run)}
    if scope == "combat":
        room = snapshot["objects"][snapshot["room"]["ref"]]
        roots = {key: value for key, value in snapshot.items() if key in COMBAT_ROOTS}
        roots["monsters"] = room["fields"]["AbstractRoom.monsters"]
        roots["screen"] = combat_screen
        roots["rng"] = {key: value for key, value in snapshot["rng"].items() if key in COMBAT_RNG}
        return roots
    roots = {key: value for key, value in snapshot.items()
             if key not in COMBAT_ROOTS | {"objects", "screen", "rng"}}
    roots["screen"] = run_screen
    roots["rng"] = {key: value for key, value in snapshot["rng"].items() if key not in COMBAT_RNG}
    return roots


def compare_scope(expected, actual, scope, path="$.snapshot", identities=None):
    compare(scope_roots(expected, scope), scope_roots(actual, scope), path,
            GraphComparison(expected, actual, scope, identities))


@dataclass
class ScopeResult:
    matched_decisions: int = 0
    terminal_matches: bool = False
    differences: list = field(default_factory=list)

    @property
    def passed(self):
        return self.terminal_matches and not self.differences


@dataclass
class ReplayResult:
    scenario: bool
    decisions: int = 0
    scopes: dict = field(default_factory=lambda: {name: ScopeResult() for name in ("combat", "run")})
    stopped: str | None = None

    @property
    def passed(self):
        return self.scopes["combat"].passed and self.scopes["run"].passed


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
    with open_trace(path) as stream:
        header = json.loads(next(stream))
        compare(2, header["schema"], f"{path}:1:$.schema")
        compare(GAME_SHA256, header["game_sha256"], f"{path}:1:$.game_sha256")
        compare("THE_SILENT", header["character"], f"{path}:1:$.character")
        compare(20, header["ascension"], f"{path}:1:$.ascension")
        game = engine.Game()
        result = ReplayResult(header["scenario"] is not None)
        if result.scenario:
            game.start_combat(header["seed"], header["scenario"])
        else:
            game.reset(header["seed"])
        records = [json.loads(line) for line in stream]
        result.decisions = sum("action" in record for record in records)
        terminal = False
        for line_number, record in enumerate(records, 2):
            if terminal:
                raise ValueError(f"{path}:{line_number}: record follows terminal snapshot")
            prefix = f"{path}:{line_number}:$"
            snapshot = game.snapshot()
            legal = game.legal_actions() if "terminal" not in record else None
            identities = ({}, {}, {}, {})
            for scope, status in result.scopes.items():
                try:
                    compare_scope(record["snapshot"], snapshot, scope, f"{prefix}.snapshot", identities)
                    if scope == "combat":
                        if "terminal" in record:
                            compare(record["terminal"], terminal_reason(snapshot, result.scenario), f"{prefix}.terminal")
                        else:
                            compare(record["legal"], legal, f"{prefix}.legal")
                except Difference as error:
                    status.differences.append(str(error))
                else:
                    if "terminal" in record:
                        status.terminal_matches = True
                    else:
                        status.matched_decisions += 1
            if "terminal" in record:
                terminal = True
                continue
            if record["action"] not in legal:
                result.stopped = f"{prefix}.action: recorded action {record['action']!r} is not legal in the engine"
                return result
            game.step(record["action"])
        if not terminal:
            raise ValueError(f"{path}: terminal snapshot is missing")
    return result


def trace_paths(paths):
    for path in paths:
        if not path.is_dir():
            yield path
            continue
        candidates = sorted((*path.rglob("*.jsonl"), *path.rglob("*.jsonl.gz")))
        yield from candidates


def main(argv=None):
    parser = argparse.ArgumentParser(description="Replay original-game combat traces with exact comparisons")
    parser.add_argument("traces", nargs="*", type=Path,
                        default=[Path(__file__).parent / "traces"],
                        help="JSONL/gzip files or directories containing scenario and full-run traces")
    parser.add_argument("--module", default="spire")
    args = parser.parse_args(argv)
    engine = importlib.import_module(args.module)
    total = 0
    passed = 0
    decisions = 0
    scope_passed = {name: 0 for name in ("combat", "run")}
    scope_decisions = {name: 0 for name in scope_passed}
    for trace in trace_paths(args.traces):
        total += 1
        try:
            result = replay(trace, engine)
        except (Difference, ValueError, KeyError, StopIteration) as error:
            print(f"FAIL {trace}: {error}")
            continue
        passed += result.passed
        decisions += result.decisions
        for scope, status in result.scopes.items():
            scope_passed[scope] += status.passed
            scope_decisions[scope] += status.matched_decisions
            detail = "" if status.passed else ": " + (status.differences[0] if status.differences else result.stopped)
            print(f"{'PASS' if status.passed else 'FAIL'} {scope} {trace}: "
                  f"{status.matched_decisions}/{result.decisions} decisions{detail}")
        if result.stopped:
            print(f"STOP {trace}: {result.stopped}")
    if not total:
        raise ValueError("No combat JSONL traces found")
    print("; ".join(f"{scope}: {scope_passed[scope]}/{total} traces passed, "
                    f"{scope_decisions[scope]}/{decisions} matched decisions" for scope in scope_passed))
    return int(passed != total)


if __name__ == "__main__":
    raise SystemExit(main())
