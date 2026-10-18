import argparse
from collections import Counter, defaultdict
from copy import deepcopy
from dataclasses import asdict
import gzip
import hashlib
import json
import math
from pathlib import Path
import random
import time

import numpy as np
import torch

from tools.evaluation import partition
from .checkpoint import engine_commit
from .metrics import summarize
from .model import ModelConfig
from .pipeline import Pipeline, PipelineConfig
from .ppo import PPOConfig
from .roots import natural_roots, read_roots, validate_training_roots
from .sampling import SCHEME, root_key


BANDS = ("k8", "k32", "k64", "k128", "k256", "k512", "act4_entry", "act3_boss", "act3_entry")
EVALUATION_INTERVAL = 10_000_000
EVALUATION_STREAM = 63003
REPEATS = 4


def family_statistics(episodes):
    groups = defaultdict(list)
    for episode in episodes:
        assert partition(episode["seed"]) in ("train", "dev")
        assert episode["outcome"] in ("victory", "death", "truncated", "fault")
        groups[episode["seed"]].append(episode)
    assert groups
    rates, upper, families = [], [], {}
    for seed, group in sorted(groups.items()):
        wins = sum(e["outcome"] == "victory" for e in group)
        unknown = sum(e["outcome"] not in ("victory", "death") for e in group)
        low, high = wins / len(group), (wins + unknown) / len(group)
        rates.append(low)
        upper.append(high)
        families[str(seed)] = dict(wins=wins, attempts=len(group), unknown=unknown, rate=low)
    mean, optimistic = float(np.mean(rates)), float(np.mean(upper))
    radius = math.sqrt(math.log(40) / (2 * len(groups)))
    return dict(families=len(groups), attempts=len(episodes), wins=sum(e["outcome"] == "victory" for e in episodes),
                faults=sum(e["outcome"] == "fault" for e in episodes),
                truncated=sum(e["outcome"] == "truncated" for e in episodes), mean=mean,
                interval95=[max(0., mean - radius), min(1., optimistic + radius)],
                interval_method="Two-sided Hoeffding bound on equally weighted independent seed-family rates",
                per_family=families)


def paired_statistics(reference, current):
    assert reference["per_family"].keys() == current["per_family"].keys()
    differences, unknown = [], 0
    for seed, before in reference["per_family"].items():
        after = current["per_family"][seed]
        assert before["attempts"] == after["attempts"]
        differences.append(after["rate"] - before["rate"])
        unknown += before["unknown"] + after["unknown"]
    mean = float(np.mean(differences))
    radius = math.sqrt(2 * math.log(40) / len(differences))
    return dict(families=len(differences), mean_change=mean, unresolved=unknown,
                interval95=[max(-1., mean - radius), min(1., mean + radius)] if unknown == 0 else [-1., 1.],
                interval_method="Two-sided Hoeffding bound on paired seed-family rate differences")


class Curriculum:
    def __init__(self, roots, far, bands, seed, state=None):
        validate_training_roots(roots)
        validate_training_roots(far)
        self.bands = list(bands)
        assert self.bands
        groups = defaultdict(lambda: defaultdict(list))
        for root in roots:
            if root["band"] in self.bands:
                groups[root["band"]][root["seed"]].append(root)
        for root in far:
            groups["far"][root["seed"]].append(root)
        self.groups = {band: dict(families) for band, families in groups.items()}
        assert all(len(self.groups[band]) >= 20 for band in [*self.bands, "far"]), "A train family may have at most 5% probability"
        self.state = deepcopy(state) if state is not None else dict(frontier=0, learned=[], draws=0,
            sampling_counts={}, decision_counts={}, family_counts={}, queues={}, total_decisions=0,
            next_evaluation=EVALUATION_INTERVAL, evaluations=0, stagnant=0, best_lower=-1., stopped=None,
            promoted_at={}, evaluation_decisions=0, evaluation_seconds=0.)
        self.rng = random.Random(seed)
        if state is not None:
            self.rng.setstate(state["rng_state"])

    @property
    def frontier(self):
        return self.bands[self.state["frontier"]]

    def draw(self, count):
        result, sources = [], []
        for _ in range(count):
            turn = self.state["draws"] % 10
            learned = self.state["learned"]
            source = "far" if turn >= 8 else "learned" if turn >= 6 and learned else "frontier"
            band = "far" if source == "far" else self.rng.choice(learned) if source == "learned" else self.frontier
            queue = self.state["queues"].setdefault(band, [])
            if not queue:
                queue.extend(sorted(self.groups[band]))
                self.rng.shuffle(queue)
            family = queue.pop()
            root = self.rng.choice(self.groups[band][family])
            result.append(root)
            sources.append(source)
            self.state["draws"] += 1
            key = f"{band}/{family}"
            self.state["family_counts"][key] = self.state["family_counts"].get(key, 0) + 1
        return result, sources

    def record(self, episodes, sources, first):
        counts = Counter()
        for episode in episodes:
            source = sources[episode["attempt"] - first]
            count = len(episode["records"])
            counts[source] += count
            self.state["sampling_counts"][source] = self.state["sampling_counts"].get(source, 0) + 1
            self.state["decision_counts"][source] = self.state["decision_counts"].get(source, 0) + count
        self.state["total_decisions"] += sum(counts.values())
        return dict(counts)

    def assess(self, summary):
        lower = summary["interval95"][0]
        self.state["evaluations"] += 1
        before = self.frontier
        if lower > .6 and self.state["frontier"] + 1 < len(self.bands):
            self.state["learned"].append(before)
            self.state["promoted_at"][before] = dict(decisions=self.state["total_decisions"], mean=summary["mean"])
            self.state["frontier"] += 1
            self.state["best_lower"], self.state["stagnant"] = -1., 0
        else:
            self.state["stagnant"] = 0 if lower > self.state["best_lower"] else self.state["stagnant"] + 1
            self.state["best_lower"] = max(lower, self.state["best_lower"])
            if self.state["stagnant"] >= 3:
                self.state["stopped"] = "three_evaluations_without_frontier_improvement"
        self.state["next_evaluation"] = (self.state["total_decisions"] // EVALUATION_INTERVAL + 1) * EVALUATION_INTERVAL
        return dict(before=before, after=self.frontier, lower95=lower, stagnant=self.state["stagnant"])

    def state_dict(self):
        return deepcopy(dict(self.state, rng_state=self.rng.getstate()))


def evaluation_sets(train, dev, e2):
    sets = {}
    for part, roots in (("train", train), ("dev", dev)):
        for band in BANDS:
            selected = sorted((r for r in roots if r["band"] == band), key=lambda r: (r["seed"], root_key(r)))
            if selected:
                sets[f"{part}/{band}"] = selected
    sets["dev/e2_all"] = [r["root"] for r in e2 if r["partition"] == "dev"]
    sets["dev/e2_act3_entry"] = [r["root"] for r in e2 if r["partition"] == "dev" and r["stage"] == "act3_entry"]
    sets["dev/neow"] = natural_roots(2000000000, 1024, "dev")
    assert all(partition(r["seed"]) == name.split("/")[0] for name, group in sets.items() for r in group)
    return sets


def assess(pipeline, sets, output, label, total, initial, deadline):
    results, count, seconds = {}, 0, 0.
    for name, roots in sets.items():
        if time.time() >= deadline - 600:
            return results, count, seconds, False
        schedule = [(root, attempt) for root in roots for attempt in range(REPEATS)]
        episodes = []
        set_decisions, set_seconds = 0, 0.
        peaks = [0] * pipeline.config.workers
        for begin in range(0, len(schedule), 2048):
            if time.time() >= deadline - 300:
                return results, count, seconds, False
            group = schedule[begin:begin + 2048]
            result = pipeline.evaluate([r for r, _ in group], EVALUATION_STREAM, [a for _, a in group])
            count += result["decisions"]
            seconds += result["seconds"]
            set_decisions += result["decisions"]
            set_seconds += result["seconds"]
            for memory in result["memory"]:
                peaks[memory["rank"]] = max(peaks[memory["rank"]], memory.get("peak_allocated_bytes", 0))
            episodes.extend({k: v for k, v in e.items() if k != "records"} for e in result["episodes"])
            assert not any(e["fault"] for e in result["episodes"]), "Evaluation engine fault; stop before training"
        summary = family_statistics(episodes)
        if name in initial:
            summary["paired_initial"] = paired_statistics(initial[name], summary)
        record = dict(label=label, set=name, training_decisions=total, summary=summary,
                      policy_version=pipeline.version, sampling_seed=EVALUATION_STREAM, repeats=REPEATS,
                      decisions=set_decisions, seconds=set_seconds, peak_allocated_bytes_by_rank=peaks)
        with (output / "learning-curve.jsonl").open("a") as stream:
            stream.write(json.dumps(record) + "\n")
        with gzip.open(output / f"eval-{label}-{name.replace('/', '-')}.jsonl.gz", "wt") as stream:
            for episode in episodes:
                stream.write(json.dumps(episode) + "\n")
        results[name] = summary
        print(json.dumps(dict(event="evaluation", **record, summary={k: v for k, v in summary.items() if k != "per_family"})), flush=True)
    return results, count, seconds, True


def main():
    parser = argparse.ArgumentParser(description="E3 fixed-budget curriculum and natural-start control")
    parser.add_argument("--roots", type=Path, required=True)
    parser.add_argument("--dev-roots", type=Path, required=True)
    parser.add_argument("--e2", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--group", choices=("A", "B"), required=True)
    parser.add_argument("--init-seed", type=int, choices=(1729, 2718, 31415), required=True)
    parser.add_argument("--decisions", type=int, default=100_000_000)
    parser.add_argument("--deadline-epoch", type=float, required=True)
    parser.add_argument("--resume", type=Path)
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    torch.manual_seed(args.init_seed)
    assert 0 < args.decisions <= 100_000_000
    train, dev = read_roots(args.roots), read_roots(args.dev_roots)
    with gzip.open(args.e2, "rt") as stream:
        e2 = [json.loads(line) for line in stream]
    far = [r["root"] for r in e2 if r["partition"] == "train" and r["stage"] == "act3_entry"]
    sets = evaluation_sets(train, dev, e2)
    bands = [band for band in BANDS if len({r["seed"] for r in train if r["band"] == band}) >= 20
             and f"dev/{band}" in sets]
    bands.sort(key=lambda band: (float(np.median([r["distance_to_win"] for r in train if r["band"] == band])),
                                 BANDS.index(band)))
    checkpoint = torch.load(args.resume, map_location="cpu", weights_only=True) if args.resume else None
    curriculum = Curriculum(train, far, bands, args.init_seed, checkpoint["curriculum"] if checkpoint else None)
    model, ppo = ModelConfig(), PPOConfig(epochs=1, minibatch_size=8192)
    config = PipelineConfig(workers=8, batch_size=8192, decision_cap=3000, storage_mib=2549, inference_chunk=512)
    source_hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in
                     (("train", args.roots), ("dev", args.dev_roots), ("e2", args.e2))}
    manifest = dict(entrypoint="train.curriculum", engine_commit=engine_commit(), torch_version=str(torch.__version__),
                    observation_version=2, memory="none", model=asdict(model), ppo=asdict(ppo), pipeline=asdict(config),
                    group=args.group, init_seed=args.init_seed, source_hashes=source_hashes, bands=bands,
                    sampling=SCHEME, repeats=REPEATS, evaluation_interval=EVALUATION_INTERVAL,
                    decision_target=args.decisions, interval="Two-sided Hoeffding over seed families; paired range [-1,1]",
                    band_order="Increasing median winning-path decisions remaining, fixed before training",
                    natural_train_seeds="fresh sequential train seeds; one per attempt", root_family_cap=.05)
    if checkpoint:
        assert checkpoint["config"] == manifest, "Resume requires identical source, environment, roots and experiment"
        assert checkpoint["curriculum"] is not None
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "config.json").write_text(json.dumps(manifest, indent=2) + "\n")
    initial = json.loads((args.resume.parent / "initial-evaluation.json").read_text()) if checkpoint else {}
    started = time.time()
    with Pipeline(model, ppo, config, args.init_seed, args.output, manifest, checkpoint) as pipeline:
        pipeline.initialize()
        pipeline.save_curriculum(curriculum.state_dict())
        if not initial:
            initial, count, seconds, completed = assess(pipeline, sets, args.output, "initial", 0, {}, args.deadline_epoch)
            curriculum.state["evaluation_decisions"] += count
            curriculum.state["evaluation_seconds"] += seconds
            if not completed:
                curriculum.state["stopped"] = "wall_budget_during_initial_evaluation"
        (args.output / "initial-evaluation.json").write_text(json.dumps(initial) + "\n")
        if not checkpoint and not curriculum.state["stopped"]:
            curriculum.state["best_lower"] = initial[f"dev/{curriculum.frontier}"]["interval95"][0]
        pipeline.save_curriculum(curriculum.state_dict())
        last_evaluated = curriculum.state.get("last_evaluated_decisions", 0)

        def evaluate_current():
            nonlocal last_evaluated
            total = curriculum.state["total_decisions"]
            results, count, seconds, completed = assess(pipeline, sets, args.output, str(total), total,
                                                      initial, args.deadline_epoch)
            curriculum.state["evaluation_decisions"] += count
            curriculum.state["evaluation_seconds"] += seconds
            if completed:
                event = curriculum.assess(results[f"dev/{curriculum.frontier}"]) if args.group == "A" else {}
                if args.group == "B":
                    curriculum.state["next_evaluation"] = (total // EVALUATION_INTERVAL + 1) * EVALUATION_INTERVAL
                elif event["before"] != event["after"]:
                    curriculum.state["best_lower"] = results[f"dev/{curriculum.frontier}"]["interval95"][0]
                print(json.dumps(dict(event="frontier", **event)), flush=True)
                last_evaluated = total
                curriculum.state["last_evaluated_decisions"] = total
            else:
                curriculum.state["stopped"] = "wall_budget_during_evaluation"
            pipeline.save_curriculum(curriculum.state_dict())

        while curriculum.state["total_decisions"] < args.decisions and curriculum.state["stopped"] is None:
            if curriculum.state["total_decisions"] >= curriculum.state["next_evaluation"]:
                evaluate_current()
                continue
            if time.time() >= args.deadline_epoch - 600:
                curriculum.state["stopped"] = "wall_budget"
                break
            begin, first = time.perf_counter(), pipeline.next_attempt
            group, sources = curriculum.draw(65536) if args.group == "A" else (
                natural_roots(1000010000 + first, 65536, "train"), ["neow"] * 65536)
            rollout = pipeline.collect(group, args.init_seed, range(first, first + len(group)),
                                       args.decisions - curriculum.state["total_decisions"])
            result = summarize(rollout)
            with gzip.open(args.output / "episodes.jsonl.gz", "at") as stream:
                for episode in rollout.episodes:
                    stream.write(json.dumps(dict(update=pipeline.version + 1, source=sources[episode["attempt"] - first],
                        **{k: v for k, v in episode.items() if k != "records"})) + "\n")
            assert result["faults"] == 0, "Training engine fault; stop without updating"
            targets = rollout.returns[:rollout.count][rollout.complete[:rollout.count]]
            assert np.array_equal(targets, np.round(targets)) and ((targets >= 0) & (targets <= 1)).all()
            probabilities = np.exp(rollout.log_probs[:rollout.count])
            result["selected_action_probability"] = dict(mean=float(probabilities.mean()),
                min=float(probabilities.min()), max=float(probabilities.max()))
            decisions_by_source = curriculum.record(rollout.episodes, sources, first)
            result["ppo"] = pipeline.update(rollout, curriculum=curriculum.state_dict())
            result.update(update=pipeline.version, cumulative_decisions=curriculum.state["total_decisions"],
                          frontier=curriculum.frontier,
                          source_episodes=dict(Counter(sources[e["attempt"] - first] for e in rollout.episodes)),
                          source_decisions=decisions_by_source)
            result["source_episode_shares"] = {name: count / len(rollout.episodes)
                for name, count in result["source_episodes"].items()}
            result["source_decision_shares"] = {name: count / rollout.count for name, count in decisions_by_source.items()}
            result["timing"] = dict(**rollout.timing, total_seconds=time.perf_counter() - begin)
            result["timing"]["decisions_per_second"] = rollout.count / result["timing"]["total_seconds"]
            with (args.output / "updates.jsonl").open("a") as stream:
                stream.write(json.dumps(result, allow_nan=False) + "\n")
            print(json.dumps(dict(event="update", update=pipeline.version, frontier=curriculum.frontier,
                decisions=curriculum.state["total_decisions"], wins=result["wins"], faults=result["faults"],
                decisions_per_second=result["timing"]["decisions_per_second"])), flush=True)
            total = curriculum.state["total_decisions"]
            if total == args.decisions:
                evaluate_current()
        if curriculum.state["stopped"] == "wall_budget" and last_evaluated != curriculum.state["total_decisions"]:
            evaluate_current()
        curriculum.state["stopped"] = curriculum.state["stopped"] or "decision_budget"
        pipeline.save_curriculum(curriculum.state_dict())
        report = dict(group=args.group, init_seed=args.init_seed, decisions=curriculum.state["total_decisions"],
            frontier=curriculum.frontier, learned=curriculum.state["learned"], stop_reason=curriculum.state["stopped"],
            evaluation_decisions=curriculum.state["evaluation_decisions"], evaluation_seconds=curriculum.state["evaluation_seconds"],
            elapsed_seconds=time.time() - started, eight_gpu_node_hours=(time.time() - started) / 3600,
            last_evaluated_decisions=last_evaluated, sampling_counts=curriculum.state["sampling_counts"],
            decision_counts=curriculum.state["decision_counts"], updates=pipeline.version)
        (args.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
