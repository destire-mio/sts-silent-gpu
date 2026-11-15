"""Compare policies on paired, determinized fights from saved pre-Boss game states."""
import argparse
from copy import deepcopy
import json
from pathlib import Path
import random
import time

import torch

from tools.e4_search_exam import SearchExam
from tools.e4_value_truth import RootCollector, load_policy, mean_se
from train.e4 import ACT, BOSS_ROOM, COMBAT_PHASE, DEATH, FLOOR, HP, PHASE, ROOM, SCREEN
from train.e4_evaluation_ledger import CARDS
from train.e4_stage12 import NoSLSearch
from train.model import SCREENS


NODES = ("before_act2_boss", "before_act3_boss")
MODES = {"plain": None, "search8x4": (8, 4), "search16x6": (16, 6)}
DEFAULT_SEEDS = range(1_900_000_000, 1_900_100_000)


class BossSearch(SearchExam):
    def __init__(self, lanes, device, seed, roots):
        draws, options = MODES["search16x6"]
        super().__init__(lanes, device, seed, combat_draws=draws, combat_options=options,
                         combat_scope="elite_boss", cards=False, map_k=0,
                         extra_storage_slots=roots)
        self.mode = "plain"

    def set_mode(self, mode, seed):
        self.mode = mode
        config = MODES[mode]
        if config is not None:
            self.combat_preview.draws, self.combat_preview.max_options = config
            self.combat_preview.rng.manual_seed(seed)
        self.stats["combat"] = dict.fromkeys(self.stats["combat"], 0)
        self.preview_faults.clear()

    def predict(self, model, slots):
        if self.mode == "plain":
            return NoSLSearch.predict(self, model, slots)
        return super().predict(model, slots)


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
def run_trials(runner, model, plan):
    search = runner.search
    outcomes = {root["slot"]: [] for root, _ in plan}
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
        while live:
            search.clock()
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
                outcomes[root["slot"]].append(dict(
                    seed=seed, outcome=outcome, end_screen=SCREENS[end[SCREEN]], end_floor=end[FLOOR],
                    remaining_hp=end[HP] if won else 0 if died else None,
                    turns=last_turn if won or died else None,
                    incomplete_reason=None if won or died else fault or "left_boss_or_stalled"))
            runner.completed.clear()
            live, before = next_live, {slot: after[slot] for slot in next_live}
    return outcomes


def summarize_node(states, count, draws, modes):
    paired = [root for root in states if all(
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
    highest_budget = modes[-1]
    zero = [root for root, wins in zip(paired, win_counts[highest_budget]) if wins == 0]
    fields = ("deck_size", "power_cards", "hp_ratio")
    zero_wins = dict(mode=highest_budget, roots=len(zero), fraction=len(zero) / len(paired) if paired else None,
                     means={field: mean_se([root[field] for root in zero])[0] for field in fields},
                     seeds=[root["seed"] for root in zero])
    return dict(requested_roots=count, captured_roots=len(states), paired_roots=len(paired),
                modes=by_mode, zero_wins=zero_wins, states=states)


@torch.no_grad()
def evaluate(model, device, seeds, counts, *, lanes, draws, seed=130, modes=tuple(MODES)):
    started = time.monotonic()
    search = BossSearch(lanes, device, seed, sum(counts.values()))
    runner = RootCollector(search, torch.Generator().manual_seed(seed))
    roots = runner.capture_roots(model, counts, seeds)
    collected = time.monotonic()
    power_ids = power_card_ids()
    rng = random.Random(seed)
    states, plan = {}, []
    for group in roots.values():
        for root in group:
            keys = rng.sample(range(2**63 - 1), draws)
            plan.extend((root, key) for key in keys)
            states[root["slot"]] = dict(root_features(search, root, power_ids), draw_seeds=keys, trials={})
    previews = {}
    for mode in modes:
        search.set_mode(mode, seed)
        mode_start = time.monotonic()
        for slot, trials in run_trials(runner, model, plan).items():
            states[slot]["trials"][mode] = trials
        previews[mode] = dict(search.stats["combat"], faults=search.preview_faults.copy(),
                              seconds=time.monotonic() - mode_start)
    return dict(draws=draws, seed=seed, lanes=lanes, seconds=time.monotonic() - started,
                root_collection_seconds=collected - started, previews=previews,
                nodes={node: summarize_node([states[r["slot"]] for r in group], counts[node], draws, modes)
                       for node, group in roots.items()})


def report(result):
    def fmt(value):
        return "NA" if value is None else f"{value:.3f}"

    lines = ["# E4 Boss probe", "",
             "Statistics use roots with every draw complete across all policies. SE clusters by root.",
             "Death contributes zero remaining HP. Turns count through the final decision turn.", "",
             "| Node | Policy | Roots (paired / captured / requested) | Wins / fights | Win rate ± SE | Mean HP | Mean turns | Roots by number of wins (0+) |",
             "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for node, group in result["nodes"].items():
        roots = f'{group["paired_roots"]} / {group["captured_roots"]} / {group["requested_roots"]}'
        for mode, stats in group["modes"].items():
            dist = ", ".join(map(str, stats["wins_per_root"]))
            lines.append(f'| {node} | {mode} | {roots} | {stats["wins"]} / {stats["trials"]} | '
                         f'{fmt(stats["win_rate"])} ± {fmt(stats["win_se"])} | {fmt(stats["mean_remaining_hp"])} | '
                         f'{fmt(stats["mean_turns"])} | {dist} |')
    lines.extend(["", f'| Node | Policy | Roots with 0 / {result["draws"]} wins | Share | Mean cards | Mean Powers | Mean starting HP ratio |',
                  "| --- | --- | --- | --- | --- | --- | --- |"])
    for node, group in result["nodes"].items():
        zero = group["zero_wins"]
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
    parser.add_argument("--roots", type=int, default=256, help="Roots per Boss node")
    parser.add_argument("--draws", type=int, default=4, help="Paired hidden-state draws per root")
    parser.add_argument("--lanes", type=int, default=8)
    parser.add_argument("--output", type=Path, required=True, help="Directory for JSON results and Markdown table")
    args = parser.parse_args()
    if min(args.roots, args.draws, args.lanes) < 1:
        parser.error("--roots, --draws and --lanes must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(1)
    device = torch.device(args.device)
    model = load_policy(args.model, device)
    result = evaluate(model, device, args.seeds, dict.fromkeys(NODES, args.roots),
                      lanes=args.lanes, draws=args.draws)
    result.update(model=str(args.model), device=str(device), seed_start=args.seeds.start,
                  seed_stop_exclusive=args.seeds.stop)
    table = report(result)
    (args.output / "results.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    (args.output / "report.md").write_text(table, encoding="utf-8")
    print(table, end="")


if __name__ == "__main__":
    main()
