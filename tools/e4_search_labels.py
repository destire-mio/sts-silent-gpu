"""Play with lookahead and save CRN labels from the teacher's own decisions."""
import argparse
from collections import Counter
from functools import partial
import hashlib
import json
import math
from pathlib import Path
import time

import torch

from tools.e4_map_labels import atomic_json, label_options, pack_observations
from tools.e4_value_truth import load_policy
from train.e4 import ACT, AllocationEnded, COMBAT_PHASE, FLOOR, PHASE, ROOM, SCREEN
from train.e4_lookahead import CardRewardLookahead, LookaheadSearch
from train.e4_stage12 import NoSLCollector
from train.model import SCREENS


LABEL_FORMATS = {"combat": "e4_combat_labels_v1", "card": "e4_card_labels_v1"}
SHARD_ROWS = 128
FIRST_SEED = 1_000_200_000
EXAM_SEED_START = 2_000_000_000


class LabelWriter:
    def __init__(self, output, settings, *, cards):
        self.output = output
        self.formats = LABEL_FORMATS if cards else {"combat": LABEL_FORMATS["combat"]}
        output.mkdir(parents=True, exist_ok=True)
        for name in ("combat", "card", "pilot.json", "summary.json"):
            if (output / name).exists():
                raise FileExistsError(f"Choose a fresh label output directory: {output}")
        self.pending = {kind: [] for kind in self.formats}
        self.shards = Counter()
        self.labels = Counter()
        self.overrides = Counter()
        self.labels_by_seed = Counter()
        for kind, label_format in self.formats.items():
            directory = output / kind
            directory.mkdir()
            atomic_json(directory / "manifest.json", dict(settings, format=label_format, source=kind))

    def observe(self, search, kind, parents, reports):
        accepted = [(slot, report) for slot, report in zip(parents, reports)
                    if slot not in search.failed and report["preview_steps"] > 0
                    and "crn_scores" in report
                    and all(math.isfinite(value) for value in report["reference_logits"])
                    and all(math.isfinite(score) for scores in report["crn_scores"] for score in scores)]
        if not accepted:
            return
        slots = [slot for slot, _ in accepted]
        progress = search.progress_for(slots)
        for (slot, report), packed in zip(accepted, pack_observations(search, slots)):
            seed, path = search.provenance_record(slot)
            options = report["candidates"]
            argmax = options[0]
            reference = dict(zip(report["reference_actions"], report["reference_logits"]))
            samples = dict(zip(options, report["crn_scores"]))
            p = progress[slot]
            identity = hashlib.sha256(json.dumps([seed, path]).encode()).hexdigest()
            self.pending[kind].append(dict(id=identity, seed=seed, path=path.copy(), act=p[ACT], floor=p[FLOOR],
                fight=[p[ACT], p[FLOOR], p[ROOM], bool(search.reward_ledgers[slot].reach["act3_first_boss"])],
                progress=p, prefix_length=len(path), options=options, argmax=argmax,
                selected=report["selected"], preview_steps=report["preview_steps"],
                p_ref_logits=[reference[action] for action in options],
                p_ref_all_options=report["reference_actions"], p_ref_all_logits=report["reference_logits"],
                complete=True, packed_observation=packed, scores=samples, draws=report["seeds"],
                validated_delta=report.get("validated_delta"), validated_se=report.get("validated_se"),
                validated_k=report.get("validated_k"), validated=report.get("validated"),
                estimates=label_options(samples, argmax, options)))
            self.labels[kind] += 1
            self.labels_by_seed[seed] += 1
            self.overrides[kind] += int(report["selected"] != argmax)
        if len(self.pending[kind]) >= SHARD_ROWS:
            self.write_shard(kind)

    def write_shard(self, kind):
        rows = self.pending[kind]
        if rows:
            path = self.output / kind / f"labels-{self.shards[kind]:07d}.json.gz"
            atomic_json(path, dict(format=self.formats[kind], rows=rows))
            self.pending[kind] = []
            self.shards[kind] += 1

    def flush(self):
        for kind in self.formats:
            self.write_shard(kind)


class SearchLabelTeacher(LookaheadSearch):
    def __init__(self, lanes, device, seed, writer, *, cards, override_se=1.0, validate=0):
        super().__init__(lanes, device, seed, depth="combat", override_se=override_se, validate=validate)
        self.lookahead.on_preview = partial(writer.observe, self, "combat")
        self.card_lookahead = (CardRewardLookahead(self, seed, max_options=4, require_skip=True,
            on_preview=partial(writer.observe, self, "card")) if cards else None)
        self.pilot_deadline = float("inf")
        self.write_pilot = None
        self.next_memory_sample = 0.
        self.device_peak_used = None

    def clock(self):
        now = time.monotonic()
        if self.device.type == "cuda" and now >= self.next_memory_sample:
            free, total = torch.cuda.mem_get_info(self.device)
            self.device_peak_used = max(self.device_peak_used or 0, total - free)
            self.next_memory_sample = now + 1.
        if self.write_pilot is not None and now >= self.pilot_deadline:
            write = self.write_pilot
            self.write_pilot = None
            write()
        super().clock()

    def record_fault(self, slot, reason, extra_action=None):
        provenance = self.provenance[slot]
        source = provenance[1] if len(provenance) == 3 else slot
        if source >= self.lanes:
            self.failed[source] = reason
        else:
            super().record_fault(slot, reason, extra_action)

    def predict(self, model, slots):
        actions, values, logs = super().predict(model, slots)
        if self.card_lookahead is not None:
            progress = self.progress_for(slots)
            positions = [i for i, slot in enumerate(slots) if slot not in self.failed
                         and progress[slot][PHASE] != COMBAT_PHASE
                         and SCREENS[progress[slot][SCREEN]] == "card_reward"]
            if positions:
                selected, _ = self.card_lookahead.select(model, [slots[i] for i in positions])
                for i, action in zip(positions, selected):
                    actions[i] = action
        return actions, values, logs


def collect_teacher_labels(collector, model, seeds, writer):
    try:
        collector.evaluate(model, seeds)
    except AllocationEnded:
        return "time_budget"
    finally:
        writer.flush()
    return "seeds_exhausted"


def report_metrics(writer, collector, elapsed, device, lanes):
    completed = len(collector.completed)
    finished_labels = sum(writer.labels_by_seed[episode["seed"]] for episode in collector.completed)
    total_labels = sum(writer.labels.values())
    search = collector.search
    previews = {"combat": search.lookahead, "card": search.card_lookahead}
    sources = {}
    total_steps = 0
    for kind in writer.formats:
        n = writer.labels[kind]
        steps = previews[kind].simulated_steps
        total_steps += steps
        sources[kind] = dict(labels=n, labels_per_second=n / elapsed,
            preview_steps=steps, preview_steps_per_second=steps / elapsed,
            argmax_overrides=writer.overrides[kind], argmax_override_fraction=writer.overrides[kind] / n if n else None)
    return dict(device=str(device), lanes=lanes, seconds=elapsed, by_source=sources, labels=total_labels,
        labels_per_second=total_labels / elapsed, preview_steps=total_steps,
        preview_steps_per_second=total_steps / elapsed, episodes_started=collector.round_games,
        episodes_completed=completed, labels_from_unfinished_episodes=total_labels - finished_labels,
        mean_labels_per_completed_episode=finished_labels / completed if completed else None,
        cuda_peak_allocated_bytes=torch.cuda.max_memory_allocated(device) if device.type == "cuda" else None,
        cuda_peak_reserved_bytes=torch.cuda.max_memory_reserved(device) if device.type == "cuda" else None,
        cuda_device_used_peak_sampled_bytes=search.device_peak_used)


def seed_range(value):
    try:
        start, stop = map(int, value.split(":"))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Use a half-open seed range A:B") from exc
    if not FIRST_SEED <= start < stop <= EXAM_SEED_START:
        raise argparse.ArgumentTypeError(f"Use {FIRST_SEED} <= A < B <= {EXAM_SEED_START}")
    return range(start, stop)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("teacher", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", type=seed_range, required=True)
    parser.add_argument("--lanes", type=int, default=128)
    parser.add_argument("--pilot-seconds", type=float, default=300.)
    parser.add_argument("--max-seconds", type=float, default=10800.)
    parser.add_argument("--cards", action="store_true")
    parser.add_argument("--override-se", type=float, default=1.0,
                        help="Paired SE multiplier. A non-default multiplier or validation requires two nonzero differences.")
    parser.add_argument("--validate", type=int, default=0, help="Independent paired draws to confirm each proposed override.")
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--seed", type=int, default=210)
    args = parser.parse_args(argv)
    if args.lanes < 1 or not 0 < args.pilot_seconds <= args.max_seconds < math.inf:
        parser.error("Require positive lanes and 0 < pilot-seconds <= max-seconds")

    torch.set_num_threads(1)
    device = torch.device(args.device)
    if device.type == "cuda":
        torch.zeros(1, device=device)
        torch.cuda.reset_peak_memory_stats(device)
    started = time.monotonic()
    with args.teacher.open("rb") as stream:
        teacher_sha = hashlib.file_digest(stream, "sha256").hexdigest()
    writer = LabelWriter(args.output, dict(mode="search_teacher", teacher=str(args.teacher),
        teacher_sha256=teacher_sha, seed_start=args.seeds.start, seed_stop=args.seeds.stop,
        lanes=args.lanes, pilot_seconds=args.pilot_seconds, max_seconds=args.max_seconds,
        draw_seed=args.seed, combat_depth="combat", options=4, crn_draws=8, card_skip_included=args.cards,
        override_se=args.override_se, validate=args.validate), cards=args.cards)
    model = load_policy(args.teacher, device)
    search = SearchLabelTeacher(args.lanes, device, args.seed, writer, cards=args.cards,
                                override_se=args.override_se, validate=args.validate)
    search.deadline = started + args.max_seconds
    search.evidence_directory = args.output
    collector = NoSLCollector(search, torch.Generator().manual_seed(args.seed))

    def write_pilot():
        writer.flush()
        elapsed = time.monotonic() - started
        pilot = report_metrics(writer, collector, elapsed, device, args.lanes)
        pilot.update(requested_seconds=args.pilot_seconds, reached_requested_seconds=elapsed >= args.pilot_seconds)
        atomic_json(args.output / "pilot.json", pilot)

    search.pilot_deadline = started + args.pilot_seconds
    search.write_pilot = write_pilot
    reason = collect_teacher_labels(collector, model, args.seeds, writer)
    if search.write_pilot is not None:
        write_pilot()
    result = report_metrics(writer, collector, time.monotonic() - started, device, args.lanes)
    result["stop_reason"] = reason
    atomic_json(args.output / "summary.json", result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
