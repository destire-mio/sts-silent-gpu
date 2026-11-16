"""Compare policies on paired, determinized fights from saved pre-Boss game states."""
import argparse
from copy import deepcopy
import json
import math
from pathlib import Path
import random
import time

import torch

from tools.e4_search_exam import SearchExam
from tools.e4_value_truth import RootCollector, load_policy, mean_se
from train.e4 import ACT, BOSS_ROOM, COMBAT_PHASE, DEATH, FLOOR, HP, PHASE, ROOM, SCREEN, AllocationEnded
from train.e4_combat_plan import CombatPlanner
from train.e4_evaluation_ledger import CARDS
from train.e4_stage12 import NoSLSearch
from train.model import SCREENS


NODES = ("before_act2_boss", "before_act3_boss", "before_act3_second_boss")
MODES = {"plain": None, "search8x4": (8, 4), "search16x6": (16, 6), "plan64": 64, "plan256": 256}
DEFAULT_MODES = ("plain", "search8x4", "plan64", "plan256")
DEFAULT_SEEDS = range(1_900_000_000, 1_900_100_000)


class BossSearch(SearchExam):
    def __init__(self, lanes, device, seed, roots, *, preview_slots=32, plan_depth=256, plan_sims=None):
        super().__init__(lanes, device, seed, combat_scope="elite_boss", cards=False, map_k=0,
                         extra_storage_slots=roots, combat_preview_slots=preview_slots, room_probe_slots=128)
        self.preview_slots = preview_slots
        self.plan_depth = plan_depth
        self.plan_sims = plan_sims
        self.set_mode("plain", seed)

    def set_mode(self, mode, seed):
        config = MODES[mode]
        self.mode = mode
        self.planner = None
        if isinstance(config, int):
            self.planner = CombatPlanner(self, seed, sims=self.plan_sims or config, depth=self.plan_depth,
                                         batch_slots=self.preview_slots, on_fault=self.record_plan_fault)
        elif config is not None:
            self.combat_preview.draws, self.combat_preview.max_options = config
            self.combat_preview.rng.manual_seed(seed)
        self.stats["combat"] = dict.fromkeys(self.stats["combat"], 0)
        self.stats["combat"].update(decisions=0, decision_seconds=0., simulations=0)
        self.preview_faults.clear()

    def record_plan_fault(self, source, slot, fault):
        self.log_preview_fault("combat_plan", source, slot, fault)

    def predict(self, model, slots):
        started = time.perf_counter()
        decisions = sum(slot not in self.failed for slot in slots)
        if self.mode == "plain" or self.planner is not None:
            actions, values, logs = NoSLSearch.predict(self, model, slots)
        else:
            actions, values, logs = super().predict(model, slots)
        stats = self.stats["combat"]
        if self.planner is not None:
            progress = self.progress_for(slots)
            positions = [i for i, slot in enumerate(slots) if slot not in self.failed
                         and progress[slot][PHASE] == COMBAT_PHASE and progress[slot][ROOM] == BOSS_ROOM]
            parents = [slots[i] for i in positions]
            selected, reports = self.planner.select(model, parents)
            self.record_previews("combat", parents, reports)
            stats["failed_previews"] += sum(report["failed"] for report in reports)
            stats["simulations"] += sum(report["attempted"] for report in reports)
            for i, action in zip(positions, selected):
                stats["overrides"] += int(actions[i] != action)
                actions[i] = action
        stats["decisions"] += decisions
        stats["decision_seconds"] += time.perf_counter() - started
        return actions, values, logs


def power_card_ids():
    repo = Path(__file__).resolve().parents[1]
    inventory = json.loads((repo / "inventory/units.json").read_text(encoding="utf-8"))
    types = {unit["game_id"]: unit["facts"]["type"] for unit in inventory["units"] if unit["kind"] == "card"}
    return {i for i, name in enumerate(CARDS[1:], 1) if types[name] == "POWER"}


def root_features(search, root, power_ids):
    slot = root["slot"]
    tags = search.arrays["tags"][slot]
    master_deck = (tags[0] == 3) & (tags[1] == 1)  # Kind::Card, Zone::Master
    deck = search.arrays["ids"][slot, 0, master_deck].tolist()
    max_hp = int(search.arrays["values"][slot, 1, 1])  # Player token follows the screen token.
    return dict(seed=root["seed"], pre_boss_floor=root["progress"][FLOOR], deck_size=len(deck),
                power_cards=sum(card in power_ids for card in deck), hp_ratio=root["progress"][HP] / max_hp)


@torch.no_grad()
def run_trials(runner, model, plan, *, enter_boss=False, outcomes=None):
    search = runner.search
    if outcomes is None:
        outcomes = {}
    for root, _ in plan:
        outcomes.setdefault(root["slot"], [])
    turns = torch.empty(search.size, dtype=torch.int64, device=search.device)
    for offset in range(0, len(plan), search.lanes):
        chunk = plan[offset:offset + search.lanes]
        live = list(range(len(chunk)))
        runner.reset(live, [root["seed"] for root, _ in chunk])
        search.clone([root["slot"] for root, _ in chunk], live)
        search.batch.determinize_slots(live, [seed for _, seed in chunk], True)
        search.check(live)
        for slot, key in zip(live, search.loop_keys(live)):
            search.seen[slot].add(key)
            search.mask_cache.pop(slot, None)
            search.repeat_blocked.pop(slot, None)
            runner.rewards[slot] = deepcopy(chunk[slot][0]["ledger"])

        before = search.progress_for(live)
        entering = enter_boss
        while live:
            search.clock()
            if entering:
                actions = [chunk[slot][0]["entry_action"] for slot in live]
                entering = False
            else:
                actions, _, _, _ = runner.choose(model, live, before)
            search.batch.combat_turns_tensors(turns)
            turn = turns[live].tolist()
            search.last_step_progress = {}
            _, terminal = runner.advance(live, actions, before)
            after = before | search.last_step_progress
            next_live = []
            for slot, last_turn, done in zip(live, turn, terminal):
                start, end = before[slot], after[slot]
                boss = start[ROOM] == BOSS_ROOM and start[PHASE] == COMBAT_PHASE
                won = boss and (start[ACT], start[FLOOR]) in runner.rewards[slot].bosses
                exited_boss = boss and end[PHASE] != COMBAT_PHASE
                if not (done or exited_boss or won):
                    next_live.append(slot)
                    continue
                fault = search.failed.get(slot)
                died = boss and end[SCREEN] == DEATH and fault is None
                outcome = "win" if won else "death" if died else "incomplete"
                root, seed = chunk[slot]
                result = dict(seed=seed, outcome=outcome, end_screen=SCREENS[end[SCREEN]], end_floor=end[FLOOR],
                    remaining_hp=end[HP] if won else 0 if died else None, turns=last_turn if won or died else None,
                    incomplete_reason=None if won or died else fault or "left_boss_or_stalled")
                if enter_boss:
                    max_hp = int(search.arrays["values"][slot, 1, 1]) if won else None
                    result["remaining_hp_ratio"] = end[HP] / max_hp if won else 0. if died else None
                outcomes[root["slot"]].append(result)
            runner.completed.clear()
            live, before = next_live, {slot: after[slot] for slot in next_live}
    return outcomes


def summarize_node(states, count, draws, modes):
    paired = [root for root in states if modes and all(
        len(root["trials"][mode]) == draws and
        {trial["seed"] for trial in root["trials"][mode]} == set(root["draw_seeds"]) and
        all(trial["outcome"] != "incomplete" for trial in root["trials"][mode]) for mode in modes)]
    by_mode = {}
    win_counts = {}
    for mode in modes:
        win_counts[mode] = [sum(trial["outcome"] == "win" for trial in root["trials"][mode]) for root in paired]
        trials = [trial for root in paired for trial in root["trials"][mode]]
        win_rate, win_se = mean_se([wins / draws for wins in win_counts[mode]])
        by_mode[mode] = dict(win_rate=win_rate, win_se=win_se, wins=sum(win_counts[mode]), trials=len(trials),
                             mean_remaining_hp=mean_se([t["remaining_hp"] for t in trials])[0],
                             mean_turns=mean_se([t["turns"] for t in trials])[0],
                             wins_per_root=[win_counts[mode].count(n) for n in range(draws + 1)],
                             incomplete_trials=sum(t["outcome"] == "incomplete"
                                                   for row in states for t in row["trials"][mode]))
    paired_changes = {}
    for mode in modes:
        for baseline in ("plain", "search8x4"):
            if baseline not in win_counts or mode in ("plain", baseline):
                continue
            changes = [(a - b) / draws for a, b in zip(win_counts[mode], win_counts[baseline])]
            mean, se = mean_se(changes)
            paired_changes[f"{mode}_vs_{baseline}"] = dict(mean=mean, se=se, roots=len(changes))
    zero_wins = None
    if modes:
        highest_budget = modes[-1]
        zero = [root for root, wins in zip(paired, win_counts[highest_budget]) if wins == 0]
        fields = ("deck_size", "power_cards", "hp_ratio")
        zero_wins = dict(mode=highest_budget, roots=len(zero), fraction=len(zero) / len(paired) if paired else None,
                         means={field: mean_se([root[field] for root in zero])[0] for field in fields},
                         seeds=[root["seed"] for root in zero])
    return dict(requested_roots=count, captured_roots=len(states), paired_roots=len(paired),
                modes=by_mode, paired_changes=paired_changes, zero_wins=zero_wins, states=states,
                availability="available" if states else "unavailable")


def write_results(output, result):
    files = {"report.md": report(result), "results.json": json.dumps(result, indent=2, allow_nan=False) + "\n"}
    for name, content in files.items():
        target = output / name
        temporary = target.with_suffix(target.suffix + ".tmp")
        temporary.write_text(content, encoding="utf-8", newline="\n")
        temporary.replace(target)


def log_progress(phase, event, done, total, seconds, **details):
    print(json.dumps(dict(event="boss_probe_progress", phase=phase, milestone=event,
                          completed=done, total=total, seconds=round(seconds, 3), **details)), flush=True)


@torch.no_grad()
def evaluate(model, device, seeds, counts, *, lanes, draws, seed=130, modes=DEFAULT_MODES,
             max_seconds=None, output=None, metadata=None, started=None, preview_slots=32,
             plan_depth=256, plan_sims=None):
    started = time.monotonic() if started is None else started
    deadline = float("inf") if max_seconds is None else started + max_seconds
    roots = {node: [] for node in counts}
    preview_seed = random.Random(f"boss-probe-preview:{seed}").getrandbits(63)
    result = dict(metadata or {}, draws=draws, seed=seed, preview_seed=preview_seed, lanes=lanes, status="running",
                  seconds=0., root_collection_seconds=0., max_seconds=max_seconds, preview_slots=preview_slots,
                  plan_depth=plan_depth, plan_sims=plan_sims, requested_modes=list(modes), stages=[], previews={}, nodes={})
    if device.type == "cuda":
        torch.cuda.reset_peak_memory_stats(device)

    def save():
        result["seconds"] = time.monotonic() - started
        if device.type == "cuda":
            result["torch_peak_allocated_bytes"] = torch.cuda.max_memory_allocated(device)
            result["torch_peak_reserved_bytes"] = torch.cuda.max_memory_reserved(device)
        if output is not None:
            write_results(output, result)

    collection_started = time.monotonic()

    def collection_progress(milestone, status):
        captured = {node: len(group) for node, group in roots.items()}
        elapsed = time.monotonic() - collection_started
        result["root_collection_seconds"] = elapsed
        result["root_collection"] = dict(status=status, captured=captured, requested=counts, seconds=elapsed)
        save()
        log_progress("root_collection", milestone, sum(captured.values()), sum(counts.values()), elapsed,
                     captured=captured, requested=counts, status=status)

    collection_progress("start", "running")
    try:
        if time.monotonic() >= deadline:
            raise AllocationEnded()
        search = BossSearch(lanes, device, seed, sum(counts.values()), preview_slots=preview_slots,
                            plan_depth=plan_depth, plan_sims=plan_sims)
        result["native_slots"] = search.size
        search.deadline = deadline
        runner = RootCollector(search, torch.Generator().manual_seed(seed))
        runner.capture_roots(model, counts, seeds, roots=roots,
                             on_progress=lambda: collection_progress("progress", "running"))
    except AllocationEnded:
        result["status"] = "time_limit"
        collection_progress("end", "time_limit")
        return result
    collection_progress("end", "complete")

    power_ids = power_card_ids()
    rng = random.Random(seed)
    for node, group in roots.items():
        states, plan = [], []
        for root in group:
            keys = rng.sample(range(2**63 - 1), draws)
            plan.extend((root, key) for key in keys)
            states.append(dict(root_features(search, root, power_ids), draw_seeds=keys, trials={}))
        completed_modes = []
        for mode in modes:
            search.set_mode(mode, preview_seed)
            mode_start = time.monotonic()
            trials = {}
            status = "complete"
            log_progress("boss_fights", "start", 0, len(plan), 0., node=node, mode=mode)
            try:
                search.clock()
                run_trials(runner, model, plan, enter_boss=True, outcomes=trials)
            except AllocationEnded:
                status = result["status"] = "time_limit"
            for root, state in zip(group, states):
                state["trials"][mode] = trials.get(root["slot"], [])
            if status == "complete":
                completed_modes.append(mode)
            elapsed = time.monotonic() - mode_start
            finished = sum(len(rows) for rows in trials.values())
            counters = search.stats["combat"]
            decisions = counters["decisions"]
            decision_ms = 1000 * counters["decision_seconds"] / decisions if decisions else None
            result["stages"].append(dict(node=node, mode=mode, status=status, completed=finished, total=len(plan),
                                          seconds=elapsed, decisions=decisions, mean_decision_ms=decision_ms))
            preview = result["previews"].setdefault(mode, dict.fromkeys(counters, 0))
            for key, count in counters.items():
                preview[key] += count
            preview["faults"] = (preview.get("faults", []) + search.preview_faults)[:20]
            preview["seconds"] = preview.get("seconds", 0.) + elapsed
            preview["mean_decision_ms"] = 1000 * preview["decision_seconds"] / preview["decisions"] if preview["decisions"] else None
            result["nodes"][node] = summarize_node(states, counts[node], draws, completed_modes)
            save()
            log_progress("boss_fights", "end", finished, len(plan), elapsed, node=node, mode=mode, status=status)
            if status == "time_limit":
                return result
    result["status"] = "complete"
    save()
    return result


def report(result):
    def fmt(value):
        return "NA" if value is None else f"{value:.3f}"

    lines = ["# E4 Boss probe", ""]
    if collection := result.get("root_collection"):
        lines.extend([f'Run status **{result["status"]}** after {result["seconds"]:.1f} s.', "",
                      f'Root collection **{collection["status"]}** after {collection["seconds"]:.1f} s.'])
        for node, requested in collection["requested"].items():
            lines.append(f'- {node} captured {collection["captured"][node]} / {requested} roots.')
        lines.append("")
        for stage in result["stages"]:
            lines.append(f'- {stage["node"]} / {stage["mode"]} **{stage["status"]}**, '
                         f'{stage["completed"]} / {stage["total"]} trials recorded in {stage["seconds"]:.1f} s. '
                         f'Mean decision latency {fmt(stage.get("mean_decision_ms"))} ms.')
        lines.append("")
    lines.extend(["Statistics use roots with every draw complete across the finished policies for each node. SE clusters by root.",
             "Death contributes zero remaining HP. Turns count through the final decision turn.", "",
             "| Node | Policy | Roots (paired / captured / requested) | Wins / fights | Win rate ± SE | Mean HP | Mean turns | Roots by number of wins (0+) |",
             "| --- | --- | --- | --- | --- | --- | --- | --- |"])
    for node, group in result["nodes"].items():
        roots = f'{group["paired_roots"]} / {group["captured_roots"]} / {group["requested_roots"]}'
        for mode, stats in group["modes"].items():
            dist = ", ".join(map(str, stats["wins_per_root"]))
            lines.append(f'| {node} | {mode} | {roots} | {stats["wins"]} / {stats["trials"]} | '
                         f'{fmt(stats["win_rate"])} ± {fmt(stats["win_se"])} | {fmt(stats["mean_remaining_hp"])} | '
                         f'{fmt(stats["mean_turns"])} | {dist} |')
    lines.extend(["", "| Node | Comparison | Paired roots | Win rate change ± SE |",
                  "| --- | --- | --- | --- |"])
    for node, group in result["nodes"].items():
        if group["availability"] == "unavailable":
            lines.append(f"| {node} | unavailable | 0 | NA |")
        for comparison, change in group["paired_changes"].items():
            lines.append(f'| {node} | {comparison} | {change["roots"]} | {fmt(change["mean"])} ± {fmt(change["se"])} |')
    lines.extend(["", f'| Node | Policy | Roots with 0 / {result["draws"]} wins | Share | Mean cards | Mean Powers | Mean starting HP ratio |',
                  "| --- | --- | --- | --- | --- | --- | --- |"])
    for node, group in result["nodes"].items():
        zero = group["zero_wins"]
        if zero is None:
            continue
        means = zero["means"]
        lines.append(f'| {node} | {zero["mode"]} | {zero["roots"]} / {group["paired_roots"]} | '
                     f'{fmt(zero["fraction"])} | {fmt(means["deck_size"])} | {fmt(means["power_cards"])} | '
                     f'{fmt(means["hp_ratio"])} |')
    return "\n".join(lines) + "\n"


def seed_range(value):
    try:
        start, stop = map(int, value.split(":"))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Expected a stop-exclusive START:STOP") from exc
    if not 1_000_000_000 <= start < stop <= 2_000_000_000:
        raise argparse.ArgumentTypeError("Use the E4 training seed range [1000000000, 2000000000)")
    return range(start, stop)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--seeds", type=seed_range, default=DEFAULT_SEEDS, metavar="START:STOP")
    parser.add_argument("--roots", type=int, default=32, help="Roots per Boss node")
    parser.add_argument("--nodes", nargs="+", choices=("before_act1_boss", *NODES), default=NODES)
    parser.add_argument("--modes", nargs="+", choices=tuple(MODES), default=DEFAULT_MODES)
    parser.add_argument("--plan-sims", type=int, help="Override the planner's simulation budget for a smoke test")
    parser.add_argument("--plan-depth", type=int, default=256, help="Maximum decisions per combat simulation")
    parser.add_argument("--preview-slots", type=int, default=32, help="Bound on concurrent preview simulations")
    parser.add_argument("--draws", type=int, default=4, help="Paired hidden-state draws per root")
    parser.add_argument("--lanes", type=int, default=8)
    parser.add_argument("--max-seconds", type=float, help="Write partial results at the time limit (default: unlimited)")
    parser.add_argument("--output", type=Path, required=True, help="Directory for JSON results and Markdown table")
    args = parser.parse_args()
    if min(args.roots, args.draws, args.lanes, args.plan_depth, args.preview_slots) < 1:
        parser.error("Root, draw, lane, plan depth and preview slot counts must be positive.")
    if args.plan_sims is not None and args.plan_sims < 1:
        parser.error("--plan-sims must be positive.")
    if len(set(args.modes)) != len(args.modes) or len(set(args.nodes)) != len(args.nodes):
        parser.error("Specify each mode and Boss node at most once.")
    if args.max_seconds is not None and (not math.isfinite(args.max_seconds) or args.max_seconds <= 0):
        parser.error("--max-seconds must be finite and positive")
    args.output.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    torch.set_num_threads(1)
    device = torch.device(args.device)
    model = load_policy(args.model, device)
    metadata = dict(model=str(args.model), device=str(device), seed_start=args.seeds.start,
                    seed_stop_exclusive=args.seeds.stop)
    result = evaluate(model, device, args.seeds, dict.fromkeys(args.nodes, args.roots),
                      lanes=args.lanes, draws=args.draws, modes=args.modes, max_seconds=args.max_seconds,
                      output=args.output, metadata=metadata, started=started, preview_slots=args.preview_slots,
                      plan_depth=args.plan_depth, plan_sims=args.plan_sims)
    print(report(result), end="")


if __name__ == "__main__":
    main()
