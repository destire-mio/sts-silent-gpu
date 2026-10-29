import time

import torch

from train.e4 import COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY
from train.model import FIELDS, network_inputs


class CombatTeacher:
    def __init__(self, search, rollout_rng, horizon):
        assert horizon in ('turn', 'combat')
        self.search, self.rollout_rng, self.horizon = search, rollout_rng, horizon
        self.turn_buffer = torch.empty(search.size, dtype=torch.int64, device=search.device)

    def turns(self):
        self.search.batch.combat_turns_tensors(self.turn_buffer)
        return self.turn_buffer.cpu().tolist()

    @torch.no_grad()
    def select(self, policy, parents):
        search = self.search
        started = time.monotonic()
        search.clock()
        index = torch.tensor(parents, device=search.device)
        inputs = network_inputs({k: search.arrays[k] for k in FIELDS}, search.device, index)
        logits, _, _ = policy(inputs)
        candidates = []
        for logit, mask in zip(logits, inputs['candidate_mask']):
            legal = torch.where(mask)[0]
            order = torch.argsort(logit[legal], descending=True, stable=True)[:8]
            candidates.append(legal[order].cpu().tolist())
        selected, records = self.evaluate(policy, parents, candidates)
        elapsed = time.monotonic() - started
        for record in records:
            record.update(batch_seconds=elapsed, batch_decisions=len(parents), seconds=elapsed / len(parents))
        return selected, records

    @torch.no_grad()
    def discard_probes(self, policy, parents, records):
        search = self.search
        inputs = network_inputs({k: search.arrays[k] for k in FIELDS}, search.device,
                                torch.tensor(parents, device=search.device))
        rng = self.rollout_rng.get_state().clone()
        started = time.monotonic()
        try:
            for j, (parent, record) in enumerate(zip(parents, records)):
                selected_mean = record['candidate_mean_scores'][record['candidates'].index(record['selected'])]
                probes = []
                for action in torch.where(inputs['candidate_mask'][j] &
                                          (inputs['candidate_kinds'][j] == 5))[0].cpu().tolist():
                    entity = int(inputs['candidate_refs'][j, 0, action]) - 1
                    potion = int(inputs['ids'][j, 0, entity]) - 1
                    self.rollout_rng.set_state(rng)
                    _, trial = self.evaluate(policy, [parent], [[action]])
                    mean = trial[0]['candidate_mean_scores'][0]
                    probes.append(dict(potion_id=potion, discard_action=action, discard_mean_score=mean,
                        selected_mean_score=selected_mean, selected_minus_discard=selected_mean - mean,
                        preview_trials=trial[0]['preview_trials'], preview_steps=trial[0]['preview_steps']))
                record['potion_discard_probes'] = probes
        finally:
            self.rollout_rng.set_state(rng)
        elapsed = time.monotonic() - started
        for record in records:
            record['potion_probe_seconds'] = elapsed / len(parents)

    @torch.no_grad()
    def evaluate(self, policy, parents, candidates):
        search = self.search
        sources, first, seeds, groups = [], [], [], []
        for parent, actions in zip(parents, candidates):
            begin = len(sources)
            for action in actions:
                sources.extend([parent] * 16)
                first.extend([action] * 16)
                seeds.extend(range(16))
            groups.append((begin, len(sources)))
        assert all(0 <= p < search.lanes for p in parents)
        slots = search.take(len(sources))
        assert all(s >= search.lanes for s in slots)
        actor_rng = search.rng
        search.rng = self.rollout_rng
        try:
            search.clone(sources, slots)
            search.batch.determinize_slots(slots, seeds)
            search.check(slots)
            initial_turns = self.turns()
            search.step(slots, first)
            steps = dict.fromkeys(slots, 1)
            live = slots[:]
            terminal, boundary = [], []
            while live:
                search.clock()
                progress = search.progress()
                turns = self.turns()
                continuing = []
                for slot in live:
                    p = progress[slot]
                    if p[PHASE] != COMBAT_PHASE or p[SCREEN] in (DEATH, VICTORY):
                        terminal.append(slot)
                    elif ((self.horizon == 'turn' and turns[slot] != initial_turns[slot])
                          or (self.horizon == 'combat' and steps[slot] == 300)):
                        boundary.append(slot)
                    else:
                        assert steps[slot] < 3000, 'Teacher turn rollout did not finish'
                        continuing.append(slot)
                live = continuing
                if live:
                    actions, _, _ = search.predict(policy.run_policy, live)
                    search.step(live, actions)
                    for slot in live:
                        steps[slot] += 1
            scores = search.scores(policy.run_policy, terminal, progress)
            for begin in range(0, len(boundary), search.inference_chunk):
                group = boundary[begin:begin + search.inference_chunk]
                ix = torch.tensor(group, device=search.device)
                values = policy(network_inputs({k: search.arrays[k] for k in FIELDS}, search.device, ix))[2]
                scores.update(zip(group, values.cpu().tolist()))
            selected, records = [], []
            for parent, actions, (begin, end) in zip(parents, candidates, groups):
                trials = slots[begin:end]
                means = [sum(scores[s] for s in trials[j:j + 16]) / 16 for j in range(0, len(trials), 16)]
                best = max(range(len(actions)), key=means.__getitem__)
                selected.append(actions[best])
                records.append(dict(slot=parent, candidates=actions, candidate_mean_scores=means,
                    selected=actions[best], preview_steps=sum(steps[s] for s in trials),
                    preview_trials=len(trials), value_boundaries=sum(s in boundary for s in trials),
                    horizon=self.horizon))
            return selected, records
        finally:
            search.rng = actor_rng
            search.available.extend(slots)
