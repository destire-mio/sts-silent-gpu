"""Collect resumable CRN labels at optional card rewards."""
from functools import partial
from itertools import combinations
import math
import random
import statistics

import torch

from tools.e4_card_horizons import CARD_REWARD, card_options
from tools.e4_combat_labels import (INITIAL_DRAWS, capture_label_episodes, label_parser,
                                    label_shard, run_label_job, validate_label_arguments)
from train.e4 import BATTLE_OVER, BOSS_ROOM, COMBAT_PHASE, DEATH, HP, PHASE, ROOM, SCREEN, VICTORY
from train.model import network_inputs


FORMAT = "e4_card_labels_v1"
MAX_STEPS = 3000
MAX_POINTS_PER_EPISODE = 6


def card_root(progress):
    return progress[SCREEN] == CARD_REWARD


def optional_card_actions(search, slot, mask):
    return torch.tensor(card_options(search, [slot], mask.unsqueeze(0))[0],
                        dtype=torch.long, device=mask.device)


def capture_card_episodes(model, runner, seeds):
    episodes = capture_label_episodes(model, runner, seeds, is_root=card_root, options_at=optional_card_actions)
    for episode in episodes:
        roots = episode["roots"]
        if len(roots) > MAX_POINTS_PER_EPISODE:
            rng = random.Random(f"card-roots:{episode['seed']}")
            kept = sorted(rng.sample(range(len(roots)), MAX_POINTS_PER_EPISODE))
            episode["roots"] = [roots[index] for index in kept]
    return episodes


@torch.no_grad()
def card_rollout(search, model, plan, horizon_combats):
    slots = search.take(len(plan))
    parents, first, seeds = zip(*plan)
    first_action = dict(zip(slots, first))
    result = dict.fromkeys(slots)
    combats = dict.fromkeys(slots, 0)
    decisions = 0
    try:
        search.clone(list(parents), slots)
        search.batch.determinize_slots(slots, list(seeds), True)
        live = slots.copy()
        before = search.progress_for(live)
        for tick in range(MAX_STEPS):
            if not live:
                break
            search.clock()
            if tick == 0:
                choices = [first_action[slot] for slot in live]
            else:
                inputs = network_inputs(search.arrays, search.device, torch.tensor(live, device=search.device))
                logits = model(inputs)[0].masked_fill(~inputs["candidate_mask"], -torch.inf)
                choices = logits.argmax(-1).tolist()
            search.batch.step_slots(live, choices)
            decisions += len(live)
            after = search.progress_for(live)
            faults = search.arrays["faults"].long()[live].tolist()
            truncated = search.arrays["truncated"][live].tolist()
            remaining, alive_end = [], []
            for slot, fault, cut in zip(live, faults, truncated):
                if fault or cut:
                    continue
                p = after[slot]
                if p[HP] <= 0 or p[SCREEN] == DEATH:
                    result[slot] = 0.
                    continue
                ended = before[slot][PHASE] == COMBAT_PHASE and p[PHASE] != COMBAT_PHASE and p[BATTLE_OVER]
                combats[slot] += int(ended)
                if (p[SCREEN] == VICTORY or combats[slot] == horizon_combats
                        or (ended and before[slot][ROOM] == BOSS_ROOM)):
                    alive_end.append(slot)
                else:
                    remaining.append(slot)
            if alive_end:
                inputs = network_inputs(search.arrays, search.device, torch.tensor(alive_end, device=search.device))
                for slot, value in zip(alive_end, model(inputs)[1].tolist()):
                    result[slot] = value if math.isfinite(value) else None
            live, before = remaining, after
        return [result[slot] for slot in slots], decisions
    finally:
        for slot in slots:
            search.mainline_progress.pop(slot, None)
        search.available.extend(slots)


def label_card_shard(runner, model, records, seed, horizon_combats):
    return label_shard(runner, model, records, seed, label_format=FORMAT, draw_namespace="card",
                       rollout=partial(card_rollout, horizon_combats=horizon_combats))


def pilot_diagnostics(rows):
    correlations = []
    disagreements = complete = 0
    for row in rows:
        first_draws = [scores[:INITIAL_DRAWS] for scores in row["scores"].values()]
        usable = [draws for draws in first_draws if len(draws) == INITIAL_DRAWS
                  and None not in draws and len(set(draws)) > 1]
        correlations.extend(statistics.correlation(a, b) for a, b in combinations(usable, 2))
        if row["complete"]:
            best = max(row["estimates"], key=lambda option: option["mean"])["action"]
            disagreements += int(best != row["argmax"])
            complete += 1
    return dict(paired_option_correlation_at_k8=statistics.mean(correlations) if correlations else None,
                paired_option_pairs_at_k8=len(correlations), label_argmax_disagreements=disagreements,
                label_argmax_comparisons=complete,
                label_argmax_disagreement_fraction=disagreements / complete if complete else None)


def main(argv=None):
    parser = label_parser(__doc__)
    parser.add_argument("--horizon-combats", type=int, default=3)
    parser.set_defaults(seed=209)
    args = parser.parse_args(argv)
    validate_label_arguments(parser, args)
    if args.horizon_combats < 1:
        parser.error("--horizon-combats must be positive")
    run_label_job(args, label_format=FORMAT, capture=capture_card_episodes,
                  label=partial(label_card_shard, horizon_combats=args.horizon_combats),
                  manifest_settings=dict(max_steps=MAX_STEPS, maximum_points_per_episode=MAX_POINTS_PER_EPISODE,
                                         horizon_combats=args.horizon_combats),
                  value_target="V after the requested subsequent combat count or the next boss; death=0; capped futures excluded",
                  pilot_metrics=pilot_diagnostics)


if __name__ == "__main__":
    main()
