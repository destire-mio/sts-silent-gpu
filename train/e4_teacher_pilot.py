import time

import torch

from train.e4 import AllocationEnded, COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY
from train.e4_stage6.examination import load_roots, result_rows, group_rng_state
from train.e4_teacher import CombatTeacher


GROUPS = ('act1_elite', 'act1_boss_guardian', 'act1_boss_hexaghost',
          'act1_boss_slimeboss', 'act2_elite_boss')


@torch.no_grad()
def play_group(search, policy, scenes, mode):
    assert mode in ('B2', 'T', 'T2')
    slots = load_roots(search, scenes)
    search.rng.set_state(group_rng_state(search, scenes))
    teacher = CombatTeacher(search, torch.Generator(device=search.device).manual_seed(94),
                            'combat' if mode == 'T2' else 'turn')
    live = slots[:]
    decisions = {s: [] for s in slots}
    potion_uses = dict.fromkeys(slots, 0)
    steps = dict.fromkeys(slots, 0)
    while live:
        search.clock()
        if mode == 'B2':
            started = time.monotonic()
            actions, _, _ = search.predict(policy.run_policy, live)
            elapsed = time.monotonic() - started
            records = [dict(seconds=elapsed / len(live), batch_seconds=elapsed,
                            batch_decisions=len(live), selected=a) for a in actions]
        else:
            actions, records = teacher.select(policy, live)
            if not any(steps.values()):
                teacher.discard_probes(policy, live, records)
        for slot, action in zip(live, actions):
            potion_uses[slot] += int(search.arrays['candidate_kinds'][slot, action]) == 4
        search.step(live, actions)
        for slot, record in zip(live, records):
            decisions[slot].append(record)
            steps[slot] += 1
        progress = search.progress()
        live = [s for s in live if progress[s][PHASE] == COMBAT_PHASE
                and progress[s][SCREEN] not in (DEATH, VICTORY)]
        assert all(steps[s] < 3000 for s in live), 'Teacher real combat did not finish'
    rows = result_rows(search, policy.run_policy, slots, scenes, mode, 0)
    for slot, row in zip(slots, rows):
        row['decision_timings'] = decisions[slot]
        row['decision_seconds'] = sum(r['seconds'] for r in decisions[slot])
        row['potion_probe_seconds'] = sum(r.get('potion_probe_seconds', 0.) for r in decisions[slot])
        row['potion_uses'] = potion_uses[slot]
        entities = range(int(search.arrays['entity_counts'][slot]))
        row['potions_remaining'] = sum(int(search.arrays['tags'][slot, 0, i]) == 7 and
            int(search.arrays['tags'][slot, 1, i]) == 0 and int(search.arrays['ids'][slot, 0, i]) != 1
            for i in entities)
    return rows


def evaluate_scenes(search, policy, scenes, mode, rows, save):
    started = time.monotonic()
    count_before = len(rows)
    complete = True
    try:
        for begin in range(0, len(scenes), search.lanes):
            batch = scenes[begin:begin + search.lanes]
            result = play_group(search, policy, batch, mode)
            rows.extend(result)
            save()
    except AllocationEnded:
        complete = False
    return dict(complete=complete, elapsed_seconds=time.monotonic() - started,
                expected_scenes=len(scenes), completed_scenes=len(rows) - count_before)


def summarize(rows):
    groups = {}
    for group in GROUPS:
        selected = [row for row in rows if row['group'] == group]
        if selected:
            count = sum(len(row['actions']) for row in selected)
            seconds = sum(row['decision_seconds'] for row in selected)
            groups[group] = dict(scenes=len(selected), deaths=sum(row['death'] for row in selected),
                death_rate=sum(row['death'] for row in selected) / len(selected),
                mean_score=sum(row['score'] for row in selected) / len(selected),
                decisions=count, decision_seconds=seconds, seconds_per_decision=seconds / count)
            wins = [r for r in selected if r['won']]
            probes = [p for r in selected for p in r['decision_timings'][0].get('potion_discard_probes', [])]
            kinds = sorted({p['potion_id'] for p in probes},
                           key=lambda k:(-sum(p['potion_id'] == k for p in probes), k))[:10]
            groups[group].update(mean_potion_uses=sum(r['potion_uses'] for r in selected) / len(selected),
                won_combats=len(wins), mean_potions_remaining_when_won=
                    sum(r['potions_remaining'] for r in wins) / len(wins) if wins else None,
                potion_probe_seconds=sum(r['potion_probe_seconds'] for r in selected),
                potion_probe_count=len(probes), mean_selected_minus_discard=
                    sum(p['selected_minus_discard'] for p in probes) / len(probes) if probes else None,
                potion_probe_top10=[dict(potion_id=k, probes=sum(p['potion_id'] == k for p in probes),
                    mean_selected_minus_discard=sum(p['selected_minus_discard'] for p in probes if p['potion_id'] == k) /
                    sum(p['potion_id'] == k for p in probes)) for k in kinds])
    return groups


def paired_rows(baseline, teacher):
    def key(row):
        return row['seed'], tuple(row['prefix'])
    base = {key(row): row for row in baseline}
    paired = {}
    for group in GROUPS:
        selected = [row for row in teacher if row['group'] == group and key(row) in base]
        if selected:
            paired[group] = dict(scenes=len(selected),
                B2_minus_teacher_death_rate=sum(int(base[key(r)]['death']) - int(r['death']) for r in selected) / len(selected),
                teacher_minus_B2_mean_score=sum(r['score'] - base[key(r)]['score'] for r in selected) / len(selected))
    return paired
