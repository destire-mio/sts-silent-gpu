from hashlib import sha256
import json
import time

import torch

from train.e4 import AllocationEnded, COMBAT_PHASE, PHASE, FLOOR, ACT, ROOM, SCREEN, DEATH, VICTORY, BATTLE_OVER, SMOKED, HP
from train.e4_rewards import RewardHistory


def scene_key(scene):
    return scene['seed'], tuple(scene['prefix'])


def load_roots(search, scenes):
    slots = list(range(len(scenes)))
    search.batch.reset_to_many(slots, [s['seed'] for s in scenes], [s['prefix'] for s in scenes])
    search.check(slots)
    search.map_returned[slots] = False
    for slot, scene in zip(slots, scenes):
        search.provenance[slot] = (scene['seed'], scene['prefix'].copy())
        search.reward_history[slot] = RewardHistory()
        search.frame_cache.pop(slot, None)
        search.mask_cache.pop(slot, None)
    search.reset_seen(slots)
    progress = search.progress()
    for slot, scene in zip(slots, scenes):
        p = progress[slot]
        assert p[PHASE] == COMBAT_PHASE and p[SCREEN] not in (DEATH, VICTORY)
        assert (p[ACT], p[FLOOR], p[ROOM]) == (scene['act'], scene['floor'], scene['room'])
    return slots


def result_rows(search, policy, slots, scenes, mode, repetition):
    progress = search.progress()
    scores = search.scores(policy, slots, progress)
    result = []
    for slot, scene in zip(slots, scenes):
        p = progress[slot]
        assert p[PHASE] != COMBAT_PHASE or p[SCREEN] in (DEATH, VICTORY)
        actions = search.provenance[slot][1][len(scene['prefix']):]
        assert actions
        result.append(dict(seed=scene['seed'], prefix=scene['prefix'], group=scene['group'],
            bosses_seen=511, source_job_id=4145, mode=mode, repetition=repetition,
            death=p[SCREEN] == DEATH, score=scores[slot],
            won=bool(p[BATTLE_OVER] and not p[SMOKED] and p[HP] > 0),
            escaped=bool(p[SMOKED]), actions=actions))
    return result


@torch.no_grad()
def replay32(search, policy, scenes):
    slots = load_roots(search, scenes)
    selected = search.combat(policy, slots)
    assert all(selected[slot] is not None for slot in slots), 'No completed replay32 result; preserve caps, never count as death'
    return result_rows(search, policy, slots, scenes, 'replay32', 0)


@torch.no_grad()
def single_play(search, policy, scenes, repetition):
    slots = load_roots(search, scenes)
    live = slots[:]
    steps = dict.fromkeys(slots, 0)
    while live:
        search.clock()
        actions, _, _ = search.predict(policy, live)
        search.step(live, actions)
        for slot in live:
            steps[slot] += 1
        progress = search.progress()
        live = [slot for slot in live if progress[slot][PHASE] == COMBAT_PHASE
                and progress[slot][SCREEN] not in (DEATH, VICTORY)]
        assert all(steps[slot] <= 3000 for slot in live), 'Single-play combat over-limit; preserve, never count as death'
    return result_rows(search, policy, slots, scenes, 'single', repetition)


def examination_binding(exam, search, policy_identity, rank, world):
    assert not exam['training_eligible'], 'Dev examination must never be training eligible'
    assert exam['evaluation_accepted'], 'Provisional examination cannot be used for checkpoint comparison'
    assert exam['bosses_seen'] == 511 and search.bosses_seen == 511 and search.trials == 32
    assert 0 <= rank < world and search.lanes > 0
    scenes = [s for group in exam['groups'].values() for s in group]
    assert len({scene_key(s) for s in scenes}) == len(scenes)
    digest = sha256(json.dumps(exam, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    return dict(exam_sha256=digest, policy_identity=policy_identity, rank=rank, world=world,
                lanes=search.lanes, bosses_seen=511, trials=32), scenes[rank::world]


def group_rng_state(search, scenes):
    identity = json.dumps([scene_key(s) for s in scenes], separators=(',', ':')).encode()
    seed = int.from_bytes(sha256(identity).digest()[:8], 'little') % (2**63 - 1)
    return torch.Generator(device=search.device).manual_seed(seed).get_state()


def begin_examination(exam, search, policy_identity, rank, world):
    binding, scenes = examination_binding(exam, search, policy_identity, rank, world)
    return dict(format=1, binding=binding, scenes=scenes, begin=0, case_index=0, rows=[],
        rng_state=group_rng_state(search, scenes[:search.lanes]) if scenes else None,
        done=not scenes, attempts=0, interrupted_attempts=0, elapsed_seconds=0., interrupted_seconds=0.)


def validate_examination_cursor(cursor, exam, search, policy_identity, rank, world):
    binding, scenes = examination_binding(exam, search, policy_identity, rank, world)
    assert cursor['format'] == 1 and cursor['binding'] == binding and cursor['scenes'] == scenes
    assert 0 <= cursor['case_index'] < 5 and 0 <= cursor['begin'] <= len(scenes)
    assert cursor['done'] == (cursor['begin'] == len(scenes))
    assert cursor['begin'] % search.lanes == 0 or cursor['done']
    assert not cursor['done'] or cursor['case_index'] == 0
    current_count = min(search.lanes, len(scenes) - cursor['begin'])
    assert len(cursor['rows']) == cursor['begin'] * 5 + current_count * cursor['case_index']


@torch.no_grad()
def advance_examination(cursor, search, policy):
    assert not cursor['done']
    started = time.monotonic()
    group = cursor['scenes'][cursor['begin']:cursor['begin'] + search.lanes]
    search.rng.set_state(cursor['rng_state'])
    available = search.available.copy()
    try:
        rows = (replay32(search, policy, group) if cursor['case_index'] == 0
                else single_play(search, policy, group, cursor['case_index'] - 1))
    except AllocationEnded:
        assert not search.contexts
        search.available = available
        elapsed = time.monotonic() - started
        cursor['attempts'] += 1
        cursor['interrupted_attempts'] += 1
        cursor['elapsed_seconds'] += elapsed
        cursor['interrupted_seconds'] += elapsed
        raise
    cursor['attempts'] += 1
    cursor['elapsed_seconds'] += time.monotonic() - started
    cursor['rows'].extend(rows)
    cursor['case_index'] += 1
    if cursor['case_index'] == 5:
        cursor['begin'] += len(group)
        cursor['case_index'] = 0
    cursor['done'] = cursor['begin'] == len(cursor['scenes'])
    if not cursor['done']:
        following = cursor['scenes'][cursor['begin']:cursor['begin'] + search.lanes]
        cursor['rng_state'] = (group_rng_state(search, following) if cursor['case_index'] <= 1
                               else search.rng.get_state().clone())
    return rows
