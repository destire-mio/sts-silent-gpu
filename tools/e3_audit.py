import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import random
import time

import spire

from tools.e2_roots import STAGES, partition
from tools.e3_roots import BANDS, extract


def replay_line(line):
    assert partition(line["seed"]) == line["partition"]
    game = spire.reset(line["seed"])
    positions = dict.fromkeys(STAGES, -1)
    location, snapshot, boss = None, None, False
    for index, candidate in enumerate(line["candidates"]):
        state = game.search_state()
        current = state["act"], state["floor"]
        if current != location:
            location, snapshot = current, game.snapshot()
            room = snapshot["objects"][snapshot["room"]["ref"]]["class"]
            boss = room == "rooms.MonsterRoomBoss"
        reached = []
        if not snapshot["run"]["firstRoomChosen"] and snapshot["screen"]["type"] == "MAP":
            if state["act"] in (2, 3, 4):
                reached.append(f"act{state['act']}_entry")
        if state["combat"] and boss:
            if state["act"] in (1, 3):
                reached.append(f"act{state['act']}_boss")
            if state["act"] == 4:
                reached.append("heart_start")
                if state["turn"] in (2, 3, 5):
                    reached.append(f"heart_turn{state['turn']}")
        for stage in reached:
            if positions[stage] < 0:
                positions[stage] = index
        actions = game.legal_actions()
        assert 0 <= candidate < len(actions), (line["seed"], index, candidate)
        game.step(actions[candidate])
        assert game.fault == 0, (line["seed"], index, game.fault)
    assert game.search_state()["victory"], "Recorded winning policy path did not reach victory on the CPU"
    return dict(line, stage_positions=positions)


def main():
    parser = argparse.ArgumentParser(description="Prepare each E3 round's legal-prefix sample for original-game replay on the Mac")
    parser.add_argument("--lines", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--round", required=True)
    args = parser.parse_args()
    started = time.perf_counter()
    lines = [json.loads(line) for line in args.lines.read_text().splitlines()]
    groups = defaultdict(list)
    for line in lines:
        groups[line["partition"]].append(line)
    selected = []
    for part in ("train", "dev"):
        group = sorted(groups[part], key=lambda r: r["seed"])
        selected.extend(random.Random(f"63005/{args.round}/{part}").sample(group, min(3, len(group))))
    replayed = [replay_line(line) for line in selected]
    roots = extract(replayed)
    assert len({(r["seed"], tuple(r["candidates"])) for r in roots}) >= 20
    document = dict(observation_version=2, status="pending_mac", round=args.round,
        source_sha256=hashlib.sha256(args.lines.read_bytes()).hexdigest(),
        native_replay_decisions=sum(len(r["candidates"]) for r in replayed),
        native_replay_seconds=time.perf_counter() - started, roots=roots)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, separators=(",", ":")) + "\n")
    print(json.dumps({k: v for k, v in document.items() if k != "roots"} | {"roots": len(roots)}), flush=True)


if __name__ == "__main__":
    main()
