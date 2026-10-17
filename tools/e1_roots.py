import argparse
import collections
import gzip
import hashlib
import json
from pathlib import Path
import resource
import time

import torch

from tools.e2_roots import partition
from train.evaluate import evaluate, outcome_summary
from train.model import ModelConfig, Policy
from train.rollout import root_key


class UniformPolicy(torch.nn.Module):
    def forward(self, inputs):
        mask = inputs["candidate_mask"]
        return torch.zeros_like(mask, dtype=torch.float32).masked_fill(~mask, -torch.inf), mask[:, 0].float() * 0.5


def classify(summary):
    low, high = summary["wilson95"]
    if low >= 0.1 and high <= 0.8:
        return "inside"
    if high < 0.1 or low > 0.8:
        return "outside"
    return "uncertain"


def main():
    parser = argparse.ArgumentParser(description="Screen legal victory prefixes for the E1 terminal-reward curriculum")
    parser.add_argument("--inputs", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--candidates-per-family-band", type=int, default=16)
    parser.add_argument("--torch-threads", type=int, default=4)
    args = parser.parse_args()
    torch.set_num_threads(args.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(1729)
    device = torch.device("cpu")
    model, uniform = Policy(ModelConfig()), UniformPolicy()
    groups = collections.defaultdict(list)
    seen = set()
    sources = []
    for path in args.inputs:
        sources.append(dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        for line in path.read_text().splitlines():
            row = json.loads(line)
            if row.get("outcome") != "victory":
                continue
            assert partition(row["seed"]) == row["partition"]
            for distance in (8, 32, 128):
                assert len(row["candidates"]) > distance
                root = dict(seed=row["seed"], candidates=row["candidates"][:-distance], distance_to_win=distance)
                key = root_key(root)
                if key in seen:
                    continue
                seen.add(key)
                groups[row["partition"], distance, row["seed"]].append(dict(
                    root=root, root_key=key, winning_candidates=row["candidates"],
                    source=dict(path=str(path), stage=row["stage"], attempt=row["attempt"])))
    args.output.mkdir(parents=True, exist_ok=False)
    config = dict(sources=sources, init_seed=1729, sampling_seed=61001, uniform_sampling_seed=61002,
                  samples=[64, 256], classification="Wilson95 wholly inside or outside [0.1,0.8]; uncertain excluded",
                  candidates_per_family_band=args.candidates_per_family_band, torch_threads=args.torch_threads,
                  candidate_order="source order; first qualifying prefix per family and exact distance", groups=len(groups))
    (args.output / "config.json").write_text(json.dumps(config, indent=2) + "\n")
    selected = collections.defaultdict(list)
    all_results = []
    started, cpu_started = time.perf_counter(), time.process_time()
    decisions = replay_decisions = 0
    with gzip.open(args.output / "episodes.jsonl.gz", "wt") as episodes_file:
        for (part, distance, seed), candidates in sorted(groups.items()):
            for ordinal, candidate in enumerate(candidates[:args.candidates_per_family_band]):
                root = candidate["root"]
                results = {}
                for name, policy, stream in (("untrained", model, 61001), ("uniform", uniform, 61002)):
                    episodes = []
                    for begin, end in ((0, 64), (64, 256)):
                        result = evaluate(policy, [root] * (end - begin), device, 4096, 32, stream, list(range(begin, end)))
                        episodes.extend(result["episodes"])
                        decisions += result["decisions"]
                        replay_decisions += (end - begin) * len(root["candidates"])
                        for episode in result["episodes"]:
                            episodes_file.write(json.dumps(dict(policy=name, **episode)) + "\n")
                        summary = outcome_summary(episodes)
                        assert summary["faults"] == 0 and summary["truncated"] == 0, summary
                        if classify(summary) != "uncertain":
                            break
                    results[name] = dict(**summary, classification=classify(summary))
                accepted = results["untrained"]["classification"] == "inside"
                record = dict(partition=part, seed=seed, distance=distance, candidate_ordinal=ordinal,
                              root_key=candidate["root_key"], accepted=accepted, **results)
                all_results.append(record)
                with (args.output / "measurements.jsonl").open("a") as output:
                    output.write(json.dumps(record) + "\n")
                print(json.dumps(record), flush=True)
                if accepted:
                    selected[part].append(dict(**root, selection=record, source=candidate["source"]))
                    with (args.output / "selected-lines.jsonl").open("a") as output:
                        output.write(json.dumps(dict(partition=part, **candidate)) + "\n")
                    break
                episodes_file.flush()
    for part in ("train", "dev"):
        (args.output / f"{part}.json").write_text(json.dumps(dict(observation_version=2, roots=selected[part])) + "\n")
    summary = dict(counts={part: dict(collections.Counter(r["distance_to_win"] for r in roots))
                          for part, roots in selected.items()},
                   candidates_measured=len(all_results), decisions=decisions, replay_decisions=replay_decisions,
                   wall_seconds=time.perf_counter() - started, cpu_seconds=time.process_time() - cpu_started,
                   peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
