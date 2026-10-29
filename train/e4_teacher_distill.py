from copy import deepcopy
from hashlib import sha256
import json

import torch
import torch.nn.functional as F

from train.e4 import (SegmentCollector, AllocationEnded, COMBAT_PHASE, PHASE, ROOM, ACT, FLOOR,
                      ELITE_ROOM, BOSS_ROOM, SCREEN, DEATH, BATTLE_OVER, SMOKED, HP)
from train.e4_teacher import CombatTeacher
from train.e4_teacher_pilot import GROUPS, paired_rows
from train.e4_stage6.evaluation_worker import policy_hash
from train.model import packed_inputs, store_inputs


def training_seed(seed):
    assert 1000000000 <= seed < 2000000000, 'Examination seed cannot enter distillation'


def teacher_slots(slots, progress):
    return [s for s in slots if progress[s][PHASE] == COMBAT_PHASE
            and progress[s][ROOM] in (ELITE_ROOM, BOSS_ROOM)]


def capture(search, slots):
    index = torch.tensor(slots, device=search.device, dtype=torch.int64)
    lengths = 4 + 24 * search.arrays['entity_counts'][index].long() + 7 * search.arrays['candidate_counts'][index].long()
    offsets = torch.zeros(len(slots) + 1, device=search.device, dtype=torch.int64)
    data = torch.empty(int(lengths.sum()), device=search.device, dtype=torch.uint8)
    store_inputs(search.batch, search.arrays, index, data, offsets)
    data, offsets = data.cpu(), offsets.cpu().tolist()
    return {s: data[offsets[i]:offsets[i + 1]].clone() for i, s in enumerate(slots)}


class DistillationCollector(SegmentCollector):
    def __init__(self, search, seed_rng, rollout_rng, *, rank=0, world=1, first_seed=1000000000):
        super().__init__(search, seed_rng, rank=rank)
        self.evaluation_waiting = ()
        assert 0 <= rank < world
        training_seed(first_seed)
        self.world, self.next_seed = world, first_seed + rank
        self.teacher = CombatTeacher(search, rollout_rng, 'combat')
        self.combat_rows, self.noncombat_rows = [], []
        self.encounters = {}
        self.teacher_records = {}

    def fresh(self, slots):
        seeds = list(range(self.next_seed, self.next_seed + self.world * len(slots), self.world))
        self.reset(slots, seeds)
        self.next_seed += self.world * len(slots)

    def reset(self, slots, seeds):
        for seed in seeds:
            training_seed(seed)
        super().reset(slots, seeds)

    @torch.no_grad()
    def choose(self, model, slots, progress):
        taught = teacher_slots(slots, progress)
        ordinary = [s for s in slots if s not in taught]
        self.stalled = set()
        choices, values, logs, imitation = {}, {}, {}, {}
        if ordinary:
            result = super().choose(model.run_policy, ordinary, progress)
            for target, source in zip((choices, values, logs, imitation), result):
                target.update(zip(ordinary, source))
        actions, records = self.teacher.select(model, taught) if taught else ([], [])
        self.teacher_records = dict(zip(taught, records))
        for slot, action in zip(taught, actions):
            assert not self.pending[slot]
            choices[slot], values[slot], logs[slot], imitation[slot] = action, 0., 0., False
        return ([choices[s] for s in slots], [values[s] for s in slots],
                [logs[s] for s in slots], [imitation[s] for s in slots])

    @torch.no_grad()
    def step(self, model, remaining):
        assert remaining > 0
        search = self.search
        search.clock()
        free = [s for s, seed in enumerate(self.seeds) if seed is None]
        if free:
            self.fresh(free)
        progress = search.progress()
        slots = list(range(search.lanes))
        taught = teacher_slots(slots, progress)
        excluded = set(taught[remaining:])
        slots = [s for s in slots if s not in excluded]
        taught = teacher_slots(slots, progress)
        noncombat = [s for s in slots if progress[s][PHASE] != COMBAT_PHASE]
        for s in slots:
            training_seed(self.seeds[s])
            assert search.provenance[s][0] == self.seeds[s]
        observations = capture(search, taught + noncombat) if taught or noncombat else {}
        masks = dict(zip(noncombat, search.policy_masks(noncombat).cpu())) if noncombat else {}
        pending = []
        for s in taught:
            p = progress[s]
            identity = (self.seeds[s], p[ACT], p[FLOOR], p[ROOM])
            if identity not in self.encounters:
                self.encounters[identity] = dict(seed=self.seeds[s], prefix=self.paths[s].copy())
            pending.append(dict(seed=self.seeds[s], encounter=identity,
                                data=observations[s], slot=s, decision=len(self.paths[s])))
        actions, _, _, _ = self.choose(model, slots, progress)
        selected = dict(zip(slots, actions))
        for row in pending:
            s = row.pop('slot')
            record = self.teacher_records[s]
            row.update(action=selected[s], candidates=record['candidates'],
                       candidate_mean_scores=record['candidate_mean_scores'])
            assert row['action'] == record['selected']
        noncombat_pending = [dict(seed=self.seeds[s], data=observations[s],
                                 policy_mask=masks[s].clone(), action=selected[s]) for s in noncombat]
        self.advance(slots, actions, progress)
        self.combat_rows.extend(pending)
        self.noncombat_rows.extend(noncombat_pending)
        return len(pending)

    def dataset(self):
        return dict(combat=self.combat_rows, noncombat=self.noncombat_rows,
                    encounters=self.encounters, episodes=self.completed, next_seed=self.next_seed)


def potion_count(search, slot):
    entities = range(int(search.arrays['entity_counts'][slot]))
    return sum(int(search.arrays['tags'][slot, 0, i]) == 7
               and int(search.arrays['tags'][slot, 1, i]) == 0
               and int(search.arrays['ids'][slot, 0, i]) != 1 for i in entities)


class StudentEvaluationCollector(SegmentCollector):
    def __init__(self, search, seed_rng, *, rank=0):
        super().__init__(search, seed_rng, rank=rank)
        self.potion_discards = {}
        self.after_elite = []

    def reset(self, slots, seeds):
        super().reset(slots, seeds)
        for seed in seeds:
            self.potion_discards[seed] = 0

    def advance(self, slots, actions, progress):
        active = [(s, a, self.seeds[s]) for s, a in zip(slots, actions) if s not in self.stalled]
        for s, a, seed in active:
            self.potion_discards[seed] = self.potion_discards.get(seed, 0) + int(
                self.search.arrays['candidate_kinds'][s, a] == 5)
        before = len(self.completed)
        result = super().advance(slots, actions, progress)
        after = self.search.progress()
        for s, _, seed in active:
            p, q = progress[s], after[s]
            if p[PHASE] == COMBAT_PHASE and p[ROOM] == ELITE_ROOM and (
                    q[PHASE] != COMBAT_PHASE or q[SCREEN] == DEATH):
                self.after_elite.append(dict(seed=seed, act=p[ACT], floor=p[FLOOR],
                    potions_remaining=potion_count(self.search, s),
                    won=bool(q[BATTLE_OVER] and not q[SMOKED] and q[HP] > 0), escaped=bool(q[SMOKED])))
        for episode in self.completed[before:]:
            seed = episode['seed']
            episode['potion_discards'] = self.potion_discards[seed]
            episode['potions_after_elite'] = [r for r in self.after_elite if r['seed'] == seed]
        return result


def collect(collector, reference, target, save):
    reference.eval().requires_grad_(False)
    initial = policy_hash(reference)
    assert target > 0 and not collector.combat_rows
    interrupted = False
    saved_count = 0
    try:
        while len(collector.combat_rows) < target:
            collector.step(reference, target - len(collector.combat_rows))
            count = len(collector.combat_rows)
            if count // 1024 != saved_count // 1024:
                save(collector.dataset())
                saved_count = count
    except AllocationEnded:
        interrupted = True
    assert policy_hash(reference) == initial and all(p.grad is None for p in reference.parameters())
    data = collector.dataset()
    save(data)
    return dict(teacher_decisions=len(data['combat']), B2_noncombat_decisions=len(data['noncombat']),
                target=target, reached_target=len(data['combat']) == target,
                interrupted=interrupted, frozen_policy_sha256=initial, parameter_updates=0)


def split_encounters(rows):
    identities = {tuple(r['encounter']) for r in rows}
    assert len(identities) >= 10, 'Need at least ten collected encounters for a 9:1 split'
    ordered = sorted(identities, key=lambda k: sha256(json.dumps(k).encode()).digest())
    validation = set(ordered[:len(ordered) // 10])
    train, val = [], []
    for row in rows:
        training_seed(row['seed'])
        assert row['seed'] == row['encounter'][0]
        (val if tuple(row['encounter']) in validation else train).append(row)
    return train, val


def row_inputs(rows, device):
    data = torch.cat([r['data'] for r in rows]).to(device)
    offsets = torch.tensor([0] + [len(r['data']) for r in rows], device=device).cumsum(0)
    inputs = packed_inputs(data, offsets, torch.arange(len(rows), device=device), device)
    if 'policy_mask' in rows[0]:
        inputs['candidate_mask'] &= torch.stack([r['policy_mask'] for r in rows]).to(device)[:, :inputs['candidate_mask'].shape[1]]
    return inputs


def distillation_loss(student_combat, actions, student_noncombat, reference_noncombat, legal):
    assert len(student_combat) == len(student_noncombat) == len(actions)
    ce = F.cross_entropy(student_combat, actions)
    frozen = reference_noncombat.detach().masked_fill(~legal, -torch.inf)
    student = student_noncombat.masked_fill(~legal, -torch.inf)
    ref_log = frozen.log_softmax(-1)
    student_log = student.log_softmax(-1)
    delta = ref_log.masked_fill(~legal, 0) - student_log.masked_fill(~legal, 0)
    kl = (ref_log.exp() * delta).sum(-1).mean()
    return ce + kl, ce, kl


@torch.no_grad()
def agreement(student, rows, device, batch_size, clock):
    correct = 0
    for begin in range(0, len(rows), batch_size):
        clock()
        batch = rows[begin:begin + batch_size]
        logits = student(row_inputs(batch, device))[0]
        correct += int((logits.argmax(-1) == torch.tensor([r['action'] for r in batch], device=device)).sum())
    return correct / len(rows)


def fit(reference, dataset, rng, clock, save, *, batch_size=128):
    reference.eval().requires_grad_(False)
    initial = policy_hash(reference)
    student = deepcopy(reference).requires_grad_(True)
    device = next(student.parameters()).device
    train, validation = split_encounters(dataset['combat'])
    seeds = {r['seed'] for r in train}
    noncombat = [r for r in dataset['noncombat'] if r['seed'] in seeds]
    assert noncombat, 'Need B2 noncombat observations from these same collected games'
    for row in noncombat:
        training_seed(row['seed'])
    optimizer = torch.optim.Adam(student.parameters(), lr=1e-4)
    best = deepcopy(student.state_dict())
    best_agreement = agreement(student, validation, device, batch_size, clock)
    history = []
    save(dict(epoch=0, model=best, optimizer=optimizer.state_dict(), history=history,
              validation_agreement=best_agreement, best_model=best, config=deepcopy(student.config)))
    while True:
        order = torch.randperm(len(train), generator=rng).tolist()
        anchors = torch.randint(len(noncombat), (len(train),), generator=rng).tolist()
        total_ce, total_kl = 0., 0.
        for start in range(0, len(train), batch_size):
            clock()
            selected = order[start:start + batch_size]
            combat = [train[i] for i in selected]
            ordinary = [noncombat[i] for i in anchors[start:start + batch_size]]
            c_inputs, n_inputs = row_inputs(combat, device), row_inputs(ordinary, device)
            actions = torch.tensor([r['action'] for r in combat], device=device)
            with torch.no_grad():
                target = reference(n_inputs)[0]
            optimizer.zero_grad(set_to_none=True)
            loss, ce, kl = distillation_loss(student(c_inputs)[0], actions, student(n_inputs)[0], target,
                                            n_inputs['candidate_mask'])
            loss.backward()
            optimizer.step()
            total_ce += float(ce.detach()) * len(combat)
            total_kl += float(kl.detach()) * len(combat)
        train_agreement = agreement(student, train, device, batch_size, clock)
        val_agreement = agreement(student, validation, device, batch_size, clock)
        improved = val_agreement > best_agreement
        if improved:
            best, best_agreement = deepcopy(student.state_dict()), val_agreement
        history.append(dict(epoch=len(history) + 1, training_agreement=train_agreement,
                            validation_agreement=val_agreement, cross_entropy=total_ce / len(train),
                            KL_B2_to_student=total_kl / len(train), combat_rows=len(train),
                            noncombat_rows=len(anchors), validation_rows=len(validation), improved=improved))
        assert policy_hash(reference) == initial and all(p.grad is None for p in reference.parameters())
        save(dict(epoch=len(history), model=student.state_dict(), optimizer=optimizer.state_dict(),
                  history=history, best_model=best, validation_agreement=best_agreement,
                  config=deepcopy(student.config), stopped_on_plateau=not improved))
        if not improved:
            student.load_state_dict(best, strict=True)
            return student.eval().requires_grad_(False), history


def examination_gate(baseline, student):
    key = lambda r: (r['seed'], tuple(r['prefix']))
    assert len(baseline) == len(student) == 990
    assert len({key(r) for r in baseline}) == len({key(r) for r in student}) == 990
    assert {key(r): r['group'] for r in baseline} == {key(r): r['group'] for r in student}
    expected = dict(zip(GROUPS, (400, 130, 130, 130, 200)))
    assert {g: sum(r['group'] == g for r in student) for g in GROUPS} == expected
    deaths = lambda rows, groups: sum(r['death'] for r in rows if r['group'] in groups)
    bosses = GROUPS[1:4]
    boss_saved = deaths(baseline, bosses) - deaths(student, bosses)
    elite_added = deaths(student, GROUPS[:1]) - deaths(baseline, GROUPS[:1])
    return dict(paired=paired_rows(baseline, student), Boss_saved_deaths=boss_saved,
                Boss_improvement_pp=boss_saved / 390 * 100, elite_added_deaths=elite_added,
                elite_deterioration_pp=elite_added / 400 * 100,
                passed=boss_saved * 100 > 5 * 390 and elite_added * 100 <= 3 * 400)
