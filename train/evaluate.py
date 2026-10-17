import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import spire
import torch

from tools.evaluation import paired_counts, wilson
from .model import ModelConfig, Policy, distribution, network_inputs, views
from .rollout import policy_rng, root_key, sample, synchronize
from .roots import natural_roots, read_roots
from .run import engine_commit


def outcome_summary(episodes):
    outcomes = [episode["outcome"] for episode in episodes]
    wins, deaths = outcomes.count("victory"), outcomes.count("death")
    unknown = len(outcomes) - wins - deaths
    return dict(attempts=len(outcomes), wins=wins, deaths=deaths, faults=outcomes.count("fault"),
                truncated=outcomes.count("truncated"),
                independent_root_families_won=len({e["seed"] for e in episodes if e["outcome"] == "victory"}),
                wilson95=wilson(wins, len(outcomes)) if not unknown else None,
                unresolved_win_rate_bounds=[wins / len(outcomes), (wins + unknown) / len(outcomes)],
                wilson95_with_unresolved_bounds=[wilson(wins, len(outcomes))[0], wilson(wins + unknown, len(outcomes))[1]])


def compare(reference, candidate):
    for key in ("sampling_seed", "decision_cap", "sampling", "observation_version", "evaluation_engine_commit"):
        assert reference[key] == candidate[key], f"Paired evaluation differs in {key}"
    old = {(e["root_key"], e["attempt"]): e for e in reference["episodes"]}
    new = {(e["root_key"], e["attempt"]): e for e in candidate["episodes"]}
    assert old.keys() == new.keys()
    assert all(old[seed]["root_key"] == new[seed]["root_key"] for seed in old)
    result = paired_counts({key: e["outcome"] for key, e in old.items()},
                           {key: e["outcome"] for key, e in new.items()})
    result["attempt_pairs"] = result.pop("roots")
    result["root_families"] = len({e["seed"] for e in reference["episodes"]})
    return result


@torch.no_grad()
def evaluate(model, roots, device, decision_cap, batch_size, sampling_seed, attempts=None):
    assert roots
    if attempts is None:
        assert len({root["seed"] for root in roots}) == len(roots), "Repeated roots require independent attempt IDs"
        attempts = [0] * len(roots)
    assert len(attempts) == len(roots)
    assert len({(root_key(root), attempt) for root, attempt in zip(roots, attempts)}) == len(roots)
    model.eval()
    batch = spire.TrainingBatch(min(batch_size, len(roots)), 1, decision_cap, [])
    arrays = views(batch)
    episodes = [dict(seed=root["seed"], root_key=root_key(root), attempt=attempt, prefix_decisions=len(root["candidates"]),
                     candidates=[], outcome=None, fault=0) for root, attempt in zip(roots, attempts)]
    generators = [policy_rng(sampling_seed, root["seed"], attempt) for root, attempt in zip(roots, attempts)]
    active, next_episode = {}, 0
    engine_seconds = inference_seconds = 0.0
    start = time.perf_counter()
    while active or next_episode < len(roots):
        free = [slot for slot in range(min(batch_size, len(roots))) if slot not in active]
        ids = list(range(next_episode, min(len(roots), next_episode + len(free))))
        if ids:
            before = time.perf_counter()
            batch.reset_to_many(free[:len(ids)], [roots[i]["seed"] for i in ids], [roots[i]["candidates"] for i in ids])
            engine_seconds += time.perf_counter() - before
            active.update(zip(free, ids))
            next_episode += len(ids)
        for slot in list(active):
            if batch.faults[slot]:
                episodes[active.pop(slot)].update(outcome="fault", fault=int(batch.faults[slot]))
        if not active:
            continue
        slots = np.array(list(active), dtype=np.int32)
        synchronize(device)
        before = time.perf_counter()
        inputs = network_inputs(arrays, device, slots)
        logits, _ = model(inputs)
        policy, _ = distribution(logits, inputs["candidate_mask"])
        selected = sample(policy, [generators[active[int(slot)]] for slot in slots])
        synchronize(device)
        inference_seconds += time.perf_counter() - before
        for slot, choice in zip(slots, selected):
            episodes[active[int(slot)]]["candidates"].append(int(choice))
        before = time.perf_counter()
        batch.step_slots(slots.tolist(), selected.tolist())
        engine_seconds += time.perf_counter() - before
        for slot in slots:
            slot = int(slot)
            if batch.faults[slot]:
                outcome = "fault"
            elif batch.truncated[slot]:
                outcome = "truncated"
            elif batch.done[slot]:
                outcome = "victory" if batch.rewards[slot] == 1 else "death"
            else:
                continue
            episodes[active.pop(slot)].update(outcome=outcome, fault=int(batch.faults[slot]))
    elapsed = time.perf_counter() - start
    decisions = sum(len(e["candidates"]) for e in episodes)
    return dict(episodes=episodes, summary=outcome_summary(episodes), decisions=decisions,
                timing=dict(total_seconds=elapsed, engine_encode_seconds=engine_seconds,
                            inference_seconds=inference_seconds, decisions_per_second=decisions / elapsed))


def main():
    parser = argparse.ArgumentParser(description="One frozen-policy attempt per seed; paired fixed PCG64 streams")
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--roots", type=Path)
    parser.add_argument("--root-index", type=int, help="Select one root, e.g. one distance on the historical E1 route")
    parser.add_argument("--first-seed", type=int, default=2000000000)
    parser.add_argument("--episodes", type=int, default=16)
    parser.add_argument("--partition", choices=("train", "dev", "locked_test"), default="dev")
    parser.add_argument("--sampling-seed", type=int, default=2718)
    parser.add_argument("--decision-cap", type=int, default=3000)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--device", default="cpu")
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    device = torch.device(args.device)
    checkpoint = torch.load(args.checkpoint, map_location=device, weights_only=True)
    assert checkpoint["config"]["observation_version"] == 2 and checkpoint["config"]["memory"] == "none"
    model = Policy(ModelConfig(**checkpoint["config"]["model"])).to(device)
    model.load_state_dict(checkpoint["model"])
    roots = read_roots(args.roots) if args.roots else natural_roots(args.first_seed, args.episodes, args.partition)
    if args.root_index is not None:
        roots = [roots[args.root_index]]
    result = evaluate(model, roots, device, args.decision_cap, args.batch_size, args.sampling_seed)
    result.update(checkpoint=str(args.checkpoint), checkpoint_sha256=hashlib.sha256(args.checkpoint.read_bytes()).hexdigest(),
                  training_engine_commit=checkpoint["engine_commit"], evaluation_engine_commit=engine_commit(),
                  observation_version=2, sampling_seed=args.sampling_seed, decision_cap=args.decision_cap,
                  sampling="PCG64 SeedSequence(stream, root_seed low32, root_seed high32, 0); inverse CDF", roots=roots)
    if args.reference:
        result["paired"] = compare(json.loads(args.reference.read_text()), result)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as output:
        output.write(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key not in ("episodes", "roots")}), flush=True)


if __name__ == "__main__":
    main()
