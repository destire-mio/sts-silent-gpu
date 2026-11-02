"""Evaluation-only, paired determinized combat previews."""
from copy import deepcopy
import math
import statistics
import torch
from train.e4 import COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY, HP
from train.e4_stage12 import NoSLSearch
from train.e4_evaluation_ledger import RUNIC_DOME
from train.model import network_inputs


class CombatLookahead:
    def __init__(self, search, seed=130, depth='turn'):
        assert depth in ('turn', 'combat')
        self.search, self.depth = search, depth
        self.rng = torch.Generator().manual_seed(seed)
        self.turns = torch.empty(search.size, dtype=torch.int64, device=search.device)

    def inputs(self, slots):
        return network_inputs(self.search.arrays, self.search.device,
                              torch.tensor(slots, device=self.search.device))

    def masked_inputs(self, slots):
        for slot in slots:
            self.search.mainline_progress.pop(slot, None)
        mask = self.search.policy_masks(slots)
        inputs = self.inputs(slots)
        inputs['candidate_mask'] &= mask[:, :inputs['candidate_mask'].shape[1]]
        return inputs

    @torch.no_grad()
    def select(self, policy, parents):
        search = self.search
        inputs = self.masked_inputs(parents)
        logits, _ = policy(inputs)
        logits = logits.masked_fill(~inputs['candidate_mask'], -torch.inf)
        actions, reports, sources, first, seeds, groups = [], [], [], [], [], []
        for i, source in enumerate(parents):
            legal = torch.where(inputs['candidate_mask'][i])[0]
            ordered = legal[torch.argsort(logits[i, legal], descending=True, stable=True)[:4]].tolist()
            argmax = ordered[0]
            actions.append(argmax)
            reports.append(dict(preview_steps=0, candidates=ordered, selected=argmax))
            owned_dome = ((search.arrays['tags'][source,0] == 6) & (search.arrays['tags'][source,1] == 0) & (search.arrays['ids'][source,0] == RUNIC_DOME)).any()
            if bool(owned_dome):
                reports[-1]['skipped'] = 'Runic Dome'
            if bool(owned_dome) or len(legal) == 1 or float(logits[i].softmax(-1)[argmax]) >= .9:
                continue
            paired = torch.randint(2**63 - 1, (8,), generator=self.rng).tolist()
            begin = len(sources)
            for action in ordered:
                sources.extend([source] * 8)
                first.extend([action] * 8)
                seeds.extend(paired)
            groups.append((i, begin, len(sources), paired))
        if not sources:
            return actions, reports
        slots = search.take(len(sources))
        assert all(source < search.lanes for source in parents)
        assert all(slot >= search.lanes for slot in slots)
        search.batch.clone_many(sources, slots)
        search.batch.determinize_slots(slots, seeds)
        search.batch.combat_turns_tensors(self.turns)
        initial_turns = dict(zip(slots, [int(self.turns[source]) for source in sources]))
        ledgers = {s:deepcopy(search.reward_ledgers[source]) for s,source in zip(slots,sources)}
        rewards = dict.fromkeys(slots,0.)
        before = search.progress_for(slots)
        search.batch.step_slots(slots, first)
        search.clock()
        after = search.progress_for(slots)
        for s in slots:rewards[s] += ledgers[s].advance(before[s],after[s])
        steps = dict.fromkeys(slots, 1)
        live = slots.copy()
        limit = 64 if self.depth == 'turn' else 256
        while live:
            search.clock()
            progress = search.progress_for(live)
            search.batch.combat_turns_tensors(self.turns)
            live = [s for s in live if progress[s][PHASE] == COMBAT_PHASE
                    and progress[s][SCREEN] not in (DEATH, VICTORY) and steps[s] < limit
                    and (self.depth == 'combat' or int(self.turns[s]) == initial_turns[s])]
            if live:
                preview = self.masked_inputs(live)
                preview_logits = policy(preview)[0].masked_fill(~preview['candidate_mask'], -torch.inf)
                before = progress
                search.batch.step_slots(live, preview_logits.argmax(-1).tolist())
                search.clock()
                after = search.progress_for(live)
                for s in live:rewards[s] += ledgers[s].advance(before[s],after[s])
                for slot in live:
                    steps[slot] += 1
        assert not bool(search.arrays['faults'][slots].any())
        assert not bool(search.arrays['truncated'][slots].any())
        progress = search.progress_for(slots)
        alive = [s for s in slots if progress[s][SCREEN] not in (DEATH, VICTORY) and progress[s][HP] > 0]
        values = dict(zip(alive, policy(self.inputs(alive))[1].tolist())) if alive else {}
        for i, begin, end, paired in groups:
            trials = slots[begin:end]
            scores = [rewards[s] + values.get(s, 0.) for s in trials]
            samples = [scores[j:j + 8] for j in range(0, len(scores), 8)]
            means = [statistics.mean(group) for group in samples]
            best = max(range(len(samples)), key=means.__getitem__)
            differences = [a - b for a, b in zip(samples[best], samples[0])]
            margin = statistics.stdev(differences) / math.sqrt(8)
            ordered = reports[i]['candidates']
            actions[i] = ordered[best] if statistics.mean(differences) > margin else ordered[0]
            reports[i].update(selected=actions[i], seeds=paired, mean_scores=means,
                paired_mean=statistics.mean(differences), paired_SE=margin,
                preview_steps=sum(steps[s] for s in trials), depth=self.depth)
        search.available.extend(slots)
        return actions, reports


class LookaheadSearch(NoSLSearch):
    def __init__(self, lanes, device, seed, depth='turn'):
        super().__init__(lanes, device, seed, storage_slots=lanes * 32, greedy=True)
        self.lookahead = CombatLookahead(self, seed, depth)
        self.preview_steps = 0
        self.mainline_decisions = 0
        self.preview_decisions = 0
        self.overrides = 0

    def predict(self, model, slots):
        actions, values, logs = super().predict(model, slots)
        self.mainline_decisions += len(slots)
        progress = self.progress_for(slots)
        positions = [i for i, slot in enumerate(slots)
                     if slot not in self.failed and progress[slot][PHASE] == COMBAT_PHASE]
        if positions:
            selected, reports = self.lookahead.select(model, [slots[i] for i in positions])
            for i, action in zip(positions, selected):
                actions[i] = action
            self.preview_steps += sum(report['preview_steps'] for report in reports)
            self.preview_decisions += sum(report['preview_steps'] > 0 for report in reports)
            self.overrides += sum(report['selected'] != report['candidates'][0] for report in reports)
        return actions, values, logs
