import argparse
import hashlib
import json
from pathlib import Path

from tools.evaluation import partition


def read_roots(path):
    document = json.loads(Path(path).read_text())
    assert document["observation_version"] == 2
    return document["roots"]


def natural_roots(first, count, expected_partition):
    roots = [{"seed": seed, "candidates": []} for seed in range(first, first + count)]
    assert all(partition(root["seed"]) == expected_partition for root in roots)
    return roots


def validate_training_roots(roots):
    for root in roots:
        if root["seed"] == 202610180018:
            assert root["candidates"], "Historical E1 route is a continuation task"
        else:
            assert partition(root["seed"]) == "train"


def route_roots(inventory, seed, distances):
    import spire

    entry = next(game for game in json.loads(Path(inventory).read_text())["games"] if game["seed"] == seed)
    assert entry["terminal"] == "victory"
    source = Path(entry["scenario"]).read_bytes()
    assert hashlib.sha256(source).hexdigest() == entry["scenario_sha256"]
    actions = json.loads(source)["actions"]
    game = spire.reset(seed)
    candidates = []
    for action in actions:
        candidates.append(game.legal_actions().index(action))
        game.step(action)
    observation = game.observation()
    assert observation[-1] == 0 and observation[1][0, 0] == 4
    assert len(candidates) == entry["decisions"]
    assert all(0 < distance < len(candidates) for distance in distances)
    return dict(observation_version=2, source_sha256=entry["scenario_sha256"],
                source_decisions=len(candidates), roots=[
                    dict(seed=seed, candidates=candidates[:-distance], distance_to_win=distance)
                    for distance in distances])


def main():
    parser = argparse.ArgumentParser(description="Replay accepted actions into visible candidate-index curriculum roots")
    parser.add_argument("--inventory", type=Path, default=Path("inventory/deep-neow.json"))
    parser.add_argument("--seed", type=int, default=202610180018)
    parser.add_argument("--distances", type=int, nargs="+", default=[8, 32, 128])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = route_roots(args.inventory, args.seed, args.distances)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result) + "\n")
    print(json.dumps(dict(source_decisions=result["source_decisions"], roots=len(result["roots"]), output=str(args.output))))


if __name__ == "__main__":
    main()
