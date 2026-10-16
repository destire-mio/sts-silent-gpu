"""Encode every oracle decision while running the unchanged two-scope comparator."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys
import types

import numpy as np
import spire

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import replay

SCREENS = ("COMBAT", "HAND_SELECT", "GRID", "VICTORY", "DEATH", "NONE", "CARD_REWARD",
           "COMBAT_REWARD", "EVENT", "MAP", "ROOM", "SHOP", "BOSS_REWARD")


class Audit:
    def __init__(self):
        self.screens = Counter()
        self.boundaries = Counter()
        self.events = Counter()
        self.rooms = Counter()
        self.peaks = Counter()
        self.decisions = 0

    def check(self, observation, legal):
        tags, ids, values, kinds, refs, entity_bits, candidate_bits, fault = observation
        entities = np.unpackbits(entity_bits, bitorder="little")
        mask = np.unpackbits(candidate_bits, bitorder="little")
        assert fault == 0, fault
        count, choices = int(entities.sum()), int(mask.sum())
        self.boundaries[SCREENS[int(ids[0, 0]) - 1]] += 1
        assert choices == len(legal)
        assert (entities[:count] == 1).all() and (entities[count:] == 0).all()
        assert (mask[:choices] == 1).all() and (mask[choices:] == 0).all()
        assert not tags[:, count:].any() and not ids[:, count:].any() and not values[:, count:].any() and not kinds[choices:].any() and not refs[:, choices:].any()
        assert (refs[:, :choices] >= 0).all() and (refs[:, :choices] <= count).all()
        assert (kinds[:choices] > 0).all()
        for i in np.flatnonzero(tags[0, :count] == 12):
            target = int(values[1, i])
            if target:
                assert target != i + 1 and 1 <= target <= count and tags[0, target - 1] == 12
                assert values[1, target - 1] == i + 1
        for i, action in enumerate(legal):
            row = refs[:, i]
            if action["type"] == "play":
                assert row[0] > 0 and tags[0, row[0] - 1] == 3
            if action["type"] == "choose":
                assert row[2] > 0
            if action.get("target", -1) >= 0:
                assert row[1] > 0 and tags[0, row[1] - 1] == 4
        self.peaks["entities"] = max(self.peaks["entities"], count)
        self.peaks["candidates"] = max(self.peaks["candidates"], choices)
        for kind, amount in Counter(map(int, tags[0, :count])).items():
            self.peaks[f"kind_{kind}"] = max(self.peaks[f"kind_{kind}"], amount)

    def decision(self, observation):
        ids = observation[1]
        screen = SCREENS[int(ids[0, 0]) - 1]
        self.screens[screen] += 1
        if screen == "EVENT":
            self.events[str(int(ids[3, 0]) - 1)] += 1
        if screen == "ROOM":
            self.rooms[str(int(ids[3, 0]) - 1)] += 1
        self.decisions += 1


def engine(audit):
    class ObservedGame:
        def __init__(self):
            self.game = spire.Game()

        def __getattr__(self, name):
            return getattr(self.game, name)

        def snapshot(self):
            self.current = self.game.observation()
            audit.check(self.current, self.game.legal_actions())
            return self.game.snapshot()

        def step(self, action):
            audit.decision(self.current)
            self.game.step(action)

    return types.SimpleNamespace(Game=ObservedGame)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--deep", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    paths = list(replay.trace_paths(args.paths))
    expected = {}
    if args.deep:
        for item in json.loads(args.deep.read_text())["games"]:
            path = Path(item["trace"])
            paths.append(path)
            expected[path] = item["gzip_sha256"]
    audit = Audit()
    results = []
    for path in paths:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if path in expected:
            assert digest == expected[path], path
        result = replay.replay(path, engine(audit))
        record = dict(path=str(path), sha256=digest, decisions=result.decisions, passed=result.passed,
                      scopes={key: dict(matched=value.matched_decisions, terminal=value.terminal_matches,
                                        differences=value.differences[:1]) for key, value in result.scopes.items()},
                      stopped=result.stopped)
        results.append(record)
        print(f'{"PASS" if result.passed else "FAIL"} {path}: {result.decisions}', flush=True)
    report = dict(traces=len(results), decisions=audit.decisions, screens=audit.screens,
                  boundaries=audit.boundaries,
                  events=audit.events, rooms=audit.rooms, peaks=audit.peaks,
                  gaps=[name for name in SCREENS if name not in audit.boundaries],
                  game_bytes=spire.game_bytes, observation_bytes=spire.observation_bytes,
                  results=results)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "results"}, indent=2))
    return int(any(not item["passed"] for item in results))


if __name__ == "__main__":
    raise SystemExit(main())
