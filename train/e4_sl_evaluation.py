import time

import torch

from train.e4 import AllocationEnded
from train.e4_sl_solver import CombatSolver, summarize, examination_calibration
from train.e4_teacher_pilot import GROUPS, paired_rows
from train.e4_stage6.examination import load_roots, group_rng_state


@torch.no_grad()
def evaluate_scenes(search, policy, scenes, rows, batches, save):
    for begin in range(0, len(scenes), search.lanes):
        group = scenes[begin:begin + search.lanes]
        slots = load_roots(search, group)
        rng = torch.Generator(device=search.device)
        rng.set_state(group_rng_state(search, group))
        try:
            result = CombatSolver(search, policy, rng).solve(slots)
        except AllocationEnded:
            return False
        for mode in rows:
            for scene, row in zip(group, result[mode]):
                rows[mode].append(dict(row, seed=scene['seed'], prefix=scene['prefix'], group=scene['group']))
        batches.append(dict(seconds=result['batch_seconds'], scenes=len(group),
            groups={g: sum(s['group'] == g for s in group) for g in GROUPS},
            predict_calls=result['predict_calls'], step_calls=result['step_calls'],
            progress_reads=result['progress_reads']))
        save()
    return True


def report(rows, batches, exam, baseline_episodes):
    grouped = {mode: summarize(data) for mode, data in rows.items()}
    for group in grouped['S']:
        left, right = grouped['S'][group], grouped['N32'][group]
        left['possible_value_exploitation'] = bool(left['won_scenes'] and right['won_scenes']
            and left['won_mean_hp_fraction'] < right['won_mean_hp_fraction']
            and left['won_mean_potions_remaining'] < right['won_mean_potions_remaining'])
    keys = {(r['seed'], tuple(r['prefix'])) for r in rows['B2']}
    scenes = [s for g in GROUPS for s in exam['groups'][g] if (s['seed'], tuple(s['prefix'])) in keys]
    by_key = {(r['seed'], tuple(r['prefix'])): r for r in rows['B2']}
    root_rows = [by_key[(s['seed'], tuple(s['prefix']))] for s in scenes]
    return dict(evaluation_only=True, parameter_updates=0, grouped=grouped,
        paired={mode: paired_rows(rows['B2'], rows[mode]) for mode in ('N32', 'S')},
        value_calibration=examination_calibration(scenes, root_rows, baseline_episodes) if scenes else None,
        batches=batches, batch_wall_seconds=sum(b['seconds'] for b in batches),
        group_participating_batch_wall_seconds={g: sum(b['seconds'] for b in batches if b['groups'][g]) for g in GROUPS},
        group_timing_scope='Batched groups overlap; participating batch times are not additive',
        actual_allocation_node_seconds=None, settlement_status='parent_final_Slurm_pending')
