import argparse
import gzip
import json
from pathlib import Path

from .run import ROOT, run
from replay import replay
import spire


def check(output, mod):
    with gzip.open(ROOT / "tests/traces/full-run-1.jsonl.gz", "rt") as stream:
        header = json.loads(next(stream))
        records = [json.loads(line) for line in stream]
    actions = [record["legal"].index(record["action"]) for record in records if "action" in record]
    script = iter(actions)

    def select(observation):
        assert observation[-1] == 0
        return next(script)

    result = run(output, mod, dict(seed=header["seed"], candidates=[]), select, True, 0,
                 dict(script="tests/traces/full-run-1.jsonl.gz", candidates=actions))
    assert result["outcome"] == "death" and result["decisions"] == len(actions), result
    replayed = replay(output / "trace.jsonl.gz", spire)
    assert replayed.passed, replayed
    print(f"REAL GAME: {len(actions)} decisions + terminal, combat=PASS run=PASS, zero divergence")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run the fixed seed-1 scripted policy through the headless watch mod")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mod", required=True, type=Path)
    args = parser.parse_args()
    check(args.output.resolve(), args.mod)
