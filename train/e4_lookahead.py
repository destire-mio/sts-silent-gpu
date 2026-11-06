"""Evaluation-only, paired determinized combat previews."""
from copy import deepcopy
import math
import statistics
import torch
from train.e4 import COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY, HP, ACT, ROOM, BOSS_ROOM, BATTLE_OVER
from train.model import SCREENS
from train.e4_stage12 import NoSLSearch
from train.e4_evaluation_ledger import RUNIC_DOME
from train.model import network_inputs


def preview_faults_present(flags, slots):
    # Native fault flags are uint32; CUDA advanced indexing needs int32.
    # Reinterpret the same four bytes, preserving every nonzero fault bit.
    return bool(flags.view(torch.int32)[slots].any())


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
        assert not preview_faults_present(search.arrays['faults'], slots)
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

    def clock(self):
        # Preview cache slots have no mainline provenance; heartbeat only lists lanes.
        progress = self.mainline_progress
        self.mainline_progress = {s:p for s,p in progress.items() if s < self.lanes}
        try:
            return super().clock()
        finally:
            self.mainline_progress = progress

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


class CardRewardLookahead(CombatLookahead):
    @torch.no_grad()
    def select(self, policy, parents):
        search = self.search
        progress = search.progress_for(parents)
        inputs = self.masked_inputs(parents)
        logits = policy(inputs)[0].masked_fill(~inputs['candidate_mask'], -torch.inf)
        actions, reports, sources, first, seeds, groups = [], [], [], [], [], []
        for i, source in enumerate(parents):
            legal = torch.where(inputs['candidate_mask'][i])[0]
            ordered = legal[torch.argsort(logits[i, legal], descending=True, stable=True)].tolist()
            selected = ordered[:5]
            skips = [a for a in ordered if int(inputs['candidate_kinds'][i, a]) == 7]
            if skips and skips[0] not in selected:
                selected[-1] = skips[0]
            argmax = ordered[0]
            actions.append(argmax)
            reports.append(dict(preview_steps=0, candidates=selected, selected=argmax))
            if (SCREENS[progress[source][SCREEN]] != 'card_reward' or len(legal) <= 1
                    or float(logits[i].softmax(-1)[argmax]) >= .9):
                continue
            paired = torch.randint(2**63 - 1, (8,), generator=self.rng).tolist()
            begin = len(sources)
            for action in selected:
                sources.extend([source] * 8)
                first.extend([action] * 8)
                seeds.extend(paired)
            groups.append((i, begin, len(sources), paired))
        if not sources:
            return actions, reports
        slots = search.take(len(sources))
        assert all(source < search.lanes for source in parents)
        assert all(slot >= search.lanes for slot in slots)
        search.clone(sources, slots)
        search.batch.determinize_slots(slots, seeds, True)
        ledgers = {s:deepcopy(search.reward_ledgers[source]) for s,source in zip(slots,sources)}
        initial_acts = {s:progress[source][ACT] for s,source in zip(slots,sources)}
        rewards, steps, combats = (dict.fromkeys(slots, value) for value in (0., 0, 0))
        boss_ended = dict.fromkeys(slots, False)
        live, choices = slots.copy(), first
        while live:
            search.clock()
            before = search.progress_for(live)
            search.step(live, choices)
            assert not preview_faults_present(search.arrays['faults'], live)
            assert not bool(search.arrays['truncated'][live].any())
            after = search.progress_for(live)
            for s in live:
                rewards[s] += ledgers[s].advance(before[s], after[s])
                steps[s] += 1
                ended = (before[s][PHASE] == COMBAT_PHASE and after[s][PHASE] != COMBAT_PHASE
                         and after[s][BATTLE_OVER])
                combats[s] += int(ended)
                boss_ended[s] |= ended and before[s][ROOM] == BOSS_ROOM
            live = [s for s in live if after[s][SCREEN] not in (DEATH, VICTORY) and after[s][HP] > 0
                    and combats[s] < 3 and not boss_ended[s] and after[s][ACT] == initial_acts[s]
                    and steps[s] < 1024]
            if live:
                preview = self.masked_inputs(live)
                choices = policy(preview)[0].masked_fill(~preview['candidate_mask'], -torch.inf).argmax(-1).tolist()
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
                preview_steps=sum(steps[s] for s in trials), depth='card')
        search.available.extend(slots)
        return actions, reports


class CardLookaheadSearch(LookaheadSearch):
    def __init__(self, lanes, device, seed):
        NoSLSearch.__init__(self, lanes, device, seed, storage_slots=lanes * 40, greedy=True)
        self.lookahead = CardRewardLookahead(self, seed)
        self.mainline_decisions = self.preview_decisions = self.overrides = self.preview_steps = 0
        self.card_reward_decisions = 0

    def record_fault(self, slot, reason, extra_action=None):
        if slot >= self.lanes:
            provenance = self.provenance[slot]
            assert len(provenance) == 3 and provenance[1] < self.lanes, reason
        return super().record_fault(slot, reason, extra_action)

    def predict(self, model, slots):
        actions, values, logs = NoSLSearch.predict(self, model, slots)
        self.mainline_decisions += len(slots)
        progress = self.progress_for(slots)
        positions = [i for i, slot in enumerate(slots) if slot not in self.failed
                     and SCREENS[progress[slot][SCREEN]] == 'card_reward']
        self.card_reward_decisions += len(positions)
        if positions:
            selected, reports = self.lookahead.select(model, [slots[i] for i in positions])
            for i, action in zip(positions, selected):
                actions[i] = action
            self.preview_steps += sum(report['preview_steps'] for report in reports)
            self.preview_decisions += sum(report['preview_steps'] > 0 for report in reports)
            self.overrides += sum(report['selected'] != report['candidates'][0] for report in reports)
        return actions, values, logs
