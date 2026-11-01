"""Evaluation-only, paired determinized combat previews."""
import math
import statistics
import torch
from train.e4 import COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY, HP
from train.e4_stage12 import NoSLSearch
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

    @torch.no_grad()
    def select(self, policy, source):
        search = self.search
        inputs = self.inputs([source])
        logits, _ = policy(inputs)
        legal = torch.where(inputs['candidate_mask'][0])[0]
        ordered = legal[torch.argsort(logits[0, legal], descending=True, stable=True)[:4]].tolist()
        argmax = ordered[0]
        if len(legal) == 1 or float(logits[0].softmax(-1)[argmax]) >= .9:
            return argmax, dict(preview_steps=0, candidates=ordered, selected=argmax)
        seeds = torch.randint(2**63 - 1, (8,), generator=self.rng).tolist()
        slots = search.take(8 * len(ordered))
        assert source < search.lanes and all(s >= search.lanes for s in slots)
        search.batch.clone_many([source] * len(slots), slots)
        search.batch.determinize_slots(slots, seeds * len(ordered))
        search.batch.combat_turns_tensors(self.turns)
        initial_turn = int(self.turns[source])
        search.batch.step_slots(slots, [action for action in ordered for _ in range(8)])
        steps = dict.fromkeys(slots, 1)
        live = slots.copy()
        limit = 64 if self.depth == 'turn' else 256
        while live:
            search.clock()
            progress = search.progress_for(live)
            search.batch.combat_turns_tensors(self.turns)
            live = [s for s in live if progress[s][PHASE] == COMBAT_PHASE
                    and progress[s][SCREEN] not in (DEATH, VICTORY) and steps[s] < limit
                    and (self.depth == 'combat' or int(self.turns[s]) == initial_turn)]
            if live:
                actions = policy(self.inputs(live))[0].argmax(-1).tolist()
                search.batch.step_slots(live, actions)
                for slot in live:
                    steps[slot] += 1
        assert not bool(search.arrays['faults'][slots].any())
        assert not bool(search.arrays['truncated'][slots].any())
        progress = search.progress_for(slots)
        alive = [s for s in slots if progress[s][SCREEN] != DEATH and progress[s][HP] > 0]
        values = dict(zip(alive, policy(self.inputs(alive))[1].tolist())) if alive else {}
        scores = [values.get(s, 0.) for s in slots]
        groups = [scores[i:i + 8] for i in range(0, len(scores), 8)]
        means = [statistics.mean(group) for group in groups]
        best = max(range(len(ordered)), key=means.__getitem__)
        differences = [a - b for a, b in zip(groups[best], groups[0])]
        margin = statistics.stdev(differences) / math.sqrt(8)
        selected = ordered[best] if statistics.mean(differences) > margin else argmax
        search.available.extend(slots)
        return selected, dict(candidates=ordered, selected=selected, seeds=seeds,
            mean_scores=means, paired_mean=statistics.mean(differences), paired_SE=margin,
            preview_steps=sum(steps.values()), depth=self.depth)


class LookaheadSearch(NoSLSearch):
    def __init__(self, lanes, device, seed, depth='turn'):
        super().__init__(lanes, device, seed, storage_slots=32, greedy=True)
        self.lookahead = CombatLookahead(self, seed, depth)
        self.preview_steps = 0

    def predict(self, model, slots):
        actions, values, logs = super().predict(model, slots)
        progress = self.progress_for(slots)
        for i, slot in enumerate(slots):
            if slot not in self.failed and progress[slot][PHASE] == COMBAT_PHASE:
                actions[i], report = self.lookahead.select(model, slot)
                self.preview_steps += report['preview_steps']
        return actions, values, logs
