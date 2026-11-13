"""Evaluation-only, paired determinized combat previews."""
from copy import deepcopy
import json
import math
import statistics
import torch
from train.e4 import COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY, HP, ACT, ROOM, BOSS_ROOM, BATTLE_OVER, MAP
from train.model import SCREENS
from train.e4_stage12 import NoSLSearch
from train.e4_evaluation_ledger import RUNIC_DOME
from train.model import network_inputs


def preview_faults_present(flags, slots):
    # Native fault flags are uint32; CUDA advanced indexing needs int32.
    # Reinterpret the same four bytes, preserving every nonzero fault bit.
    return bool(flags.view(torch.int32)[slots].any())


class CombatLookahead:
    def __init__(self, search, seed=130, depth='turn', *, on_preview=None,
                 override_se=1.0, validate=0):
        assert depth in ('turn', 'combat')
        assert math.isfinite(override_se) and override_se >= 0
        assert validate >= 0
        self.search, self.depth = search, depth
        self.on_preview = on_preview
        self.override_se = override_se
        self.validate = validate
        self.simulated_steps = 0
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

    def record_samples(self, report, samples, inputs, logits, index):
        if self.on_preview is not None:
            legal = torch.where(inputs["candidate_mask"][index])[0]
            report.update(crn_scores=samples, reference_actions=legal.tolist(),
                          reference_logits=logits[index, legal].float().tolist())

    def record_preview_fault(self, parent, slot, seed, actions, fault, entities):
        search = self.search
        directory = search.evidence_directory
        if directory is None:
            return
        path = directory / 'combat-preview-first-fault.json'
        try:
            stream = path.open('x', encoding='utf-8')
        except FileExistsError:
            return
        with stream:
            episode_seed, prefix = search.provenance_record(parent)
            collector = search.midstart_collector
            randomizations = list(collector.randomizations[parent]) if collector is not None else []
            randomizations.append((len(prefix), seed, False))
            record = dict(event='episode_fault', seed=episode_seed, candidates=prefix + actions,
                          reason=f'engine_fault:{fault}' if fault else 'empty_preview_entities',
                          slot=parent, probe_slot=slot, randomizations=randomizations,
                          start_kind=collector.starts[parent] if collector is not None else 'fresh',
                          scope='combat_preview', fault=fault, entity_count=entities)
            json.dump(record, stream)
            stream.write('\n')

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
        assert all(source < search.lanes for source in parents)
        scores, steps = self.rollout(policy, sources, first, seeds)
        validation_plan, validation_groups = [], []
        strict = self.override_se != 1.0 or self.validate > 0
        for i, begin, end, paired in groups:
            trials = scores[begin:end]
            preview_steps = sum(steps[begin:end])
            if None in trials:
                reports[i].update(preview_steps=preview_steps, skipped='combat preview fault')
                continue
            samples = [trials[j:j + 8] for j in range(0, len(trials), 8)]
            means = [statistics.mean(group) for group in samples]
            best = max(range(len(samples)), key=means.__getitem__)
            differences = [a - b for a, b in zip(samples[best], samples[0])]
            delta = statistics.mean(differences)
            margin = statistics.stdev(differences) / math.sqrt(8)
            ordered = reports[i]['candidates']
            if delta > self.override_se * margin and (not strict or sum(d != 0 for d in differences) >= 2):
                candidate = ordered[best]
                if self.validate:
                    validation_seeds = self.draw_validation_seeds(paired)
                    begin = len(validation_plan)
                    validation_plan.extend((parents[i], action, seed)
                                           for action in (ordered[0], candidate) for seed in validation_seeds)
                    validation_groups.append((i, candidate, begin, len(validation_plan)))
                else:
                    actions[i] = candidate
            reports[i].update(selected=actions[i], seeds=paired, mean_scores=means,
                              paired_mean=delta, paired_SE=margin, preview_steps=preview_steps, depth=self.depth)
            self.record_samples(reports[i], samples, inputs, logits, i)
        if validation_plan:
            self.validate_overrides(policy, validation_plan, validation_groups, actions, reports)
        if self.on_preview is not None:
            self.on_preview(parents, reports)
        return actions, reports

    def draw_validation_seeds(self, paired):
        seeds = []
        used = set(paired)
        while len(seeds) < self.validate:
            draws = torch.randint(2**63 - 1, (self.validate - len(seeds),), generator=self.rng).tolist()
            for seed in draws:
                if seed not in used:
                    seeds.append(seed)
                    used.add(seed)
        return seeds

    def validate_overrides(self, policy, plan, groups, actions, reports):
        scores, steps = [], []
        capacity = len(self.search.available)
        for start in range(0, len(plan), capacity):
            sources, first, seeds = zip(*plan[start:start + capacity])
            batch_scores, batch_steps = self.rollout(policy, list(sources), list(first), list(seeds))
            scores.extend(batch_scores)
            steps.extend(batch_steps)
        for i, candidate, begin, end in groups:
            split = begin + self.validate
            differences = [a - b for a, b in zip(scores[split:end], scores[begin:split])
                           if a is not None and b is not None]
            k = len(differences)
            complete = k == self.validate
            delta = statistics.mean(differences) if complete else None
            se = statistics.stdev(differences) / math.sqrt(k) if complete and k > 1 else None
            validated = delta is not None and delta > 0
            if validated:
                actions[i] = candidate
            reports[i].update(selected=actions[i], validated_delta=delta, validated_se=se, validated_k=k,
                              validated=validated, preview_steps=reports[i]['preview_steps'] + sum(steps[begin:end]))

    def rollout(self, policy, sources, first, seeds):
        search = self.search
        slots = search.take(len(sources))
        assert all(slot >= search.lanes for slot in slots)
        try:
            search.batch.clone_many(sources, slots)
            search.batch.determinize_slots(slots, seeds)
            search.batch.combat_turns_tensors(self.turns)
            initial_turns = dict(zip(slots, [int(self.turns[source]) for source in sources]))
            ledgers = {s:deepcopy(search.reward_ledgers[source]) for s,source in zip(slots,sources)}
            rewards = dict.fromkeys(slots,0.)
            history = {slot: [] for slot in slots}
            origin = dict(zip(slots, zip(sources, seeds)))
            failed = set()

            def healthy(active):
                if not active:
                    return []
                flags = search.arrays['faults'].view(torch.int32)[active]
                counts = search.arrays['entity_counts'][active]
                rows = torch.stack((flags, counts), dim=1).cpu().tolist()
                valid = []
                for slot, (fault, entities) in zip(active, rows):
                    if not fault and entities:
                        valid.append(slot)
                        continue
                    parent, seed = origin[slot]
                    self.record_preview_fault(parent, slot, seed, history[slot],
                                              fault & 0xFFFFFFFF, entities)
                    failed.add(slot)
                return valid

            def step(active, choices, before):
                for slot, action in zip(active, choices):
                    history[slot].append(action)
                search.batch.step_slots(active, choices)
                if self.on_preview is not None:
                    self.simulated_steps += len(active)
                valid = healthy(active)
                search.clock()
                after = search.progress_for(valid)
                for slot in valid:
                    rewards[slot] += ledgers[slot].advance(before[slot], after[slot])
                return valid

            # Cloning and determinizing do not populate the preview observation buffers.
            live = step(slots, first, search.progress_for(slots))
            limit = 64 if self.depth == 'turn' else 256
            while live:
                search.clock()
                progress = search.progress_for(live)
                search.batch.combat_turns_tensors(self.turns)
                live = [s for s in live if progress[s][PHASE] == COMBAT_PHASE
                        and progress[s][SCREEN] not in (DEATH, VICTORY) and len(history[s]) < limit
                        and (self.depth == 'combat' or int(self.turns[s]) == initial_turns[s])]
                if live:
                    preview = self.masked_inputs(live)
                    preview_logits = policy(preview)[0].masked_fill(~preview['candidate_mask'], -torch.inf)
                    live = step(live, preview_logits.argmax(-1).tolist(), progress)
            valid = [s for s in slots if s not in failed]
            truncated = search.arrays['truncated'][valid]
            if self.on_preview is None:
                assert not bool(truncated.any())
            else:
                failed.update(s for s, cut in zip(valid, truncated.tolist()) if cut)
                valid = [s for s in valid if s not in failed]
            progress = search.progress_for(valid)
            alive = [s for s in valid if progress[s][SCREEN] not in (DEATH, VICTORY) and progress[s][HP] > 0]
            values = dict(zip(alive, policy(self.inputs(alive))[1].tolist())) if alive else {}
            return ([None if s in failed else rewards[s] + values.get(s, 0.) for s in slots],
                    [len(history[s]) for s in slots])
        finally:
            search.available.extend(slots)


class LookaheadSearch(NoSLSearch):
    def __init__(self, lanes, device, seed, depth='turn', *, override_se=1.0, validate=0):
        super().__init__(lanes, device, seed, storage_slots=lanes * 32, greedy=True)
        self.lookahead = CombatLookahead(self, seed, depth, override_se=override_se, validate=validate)
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
    def __init__(self, search, seed=130, depth="turn", *, max_options=5, require_skip=False, on_preview=None):
        super().__init__(search, seed, depth, on_preview=on_preview)
        self.max_options = max_options
        self.require_skip = require_skip

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
            selected = ordered[:self.max_options]
            skips = [a for a in ordered if int(inputs['candidate_kinds'][i, a]) == 7]
            if skips and skips[0] not in selected:
                selected[-1] = skips[0]
            argmax = ordered[0]
            actions.append(argmax)
            reports.append(dict(preview_steps=0, candidates=selected, selected=argmax))
            if (SCREENS[progress[source][SCREEN]] != 'card_reward' or len(legal) <= 1
                    or (self.require_skip and not skips)
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
        failed = {s for s in slots if s in search.failed} if self.on_preview is not None else set()
        live = [s for s in slots if s not in failed]
        choices = [a for s, a in zip(slots, first) if s not in failed]
        while live:
            search.clock()
            before = search.progress_for(live)
            rejected = search.step(live, choices)
            if self.on_preview is None:
                assert not preview_faults_present(search.arrays['faults'], live)
                assert not bool(search.arrays['truncated'][live].any())
            else:
                self.simulated_steps += len(live)
                flags = search.arrays['faults'].view(torch.int32)[live].tolist()
                truncated = search.arrays['truncated'][live].tolist()
                failed.update(s for s, fault, cut in zip(live, flags, truncated)
                              if fault or cut or s in rejected or s in search.failed)
                live = [s for s in live if s not in failed]
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
        valid = [s for s in slots if s not in failed]
        progress = search.progress_for(valid)
        alive = [s for s in valid if progress[s][SCREEN] not in (DEATH, VICTORY) and progress[s][HP] > 0]
        values = dict(zip(alive, policy(self.inputs(alive))[1].tolist())) if alive else {}
        for i, begin, end, paired in groups:
            trials = slots[begin:end]
            preview_steps = sum(steps[s] for s in trials)
            if failed.intersection(trials):
                reports[i].update(preview_steps=preview_steps, skipped="card preview fault")
                continue
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
                preview_steps=preview_steps, depth='card')
            self.record_samples(reports[i], samples, inputs, logits, i)
        search.available.extend(slots)
        if self.on_preview is not None:
            for slot in slots:
                search.failed.pop(slot, None)
                search.mainline_progress.pop(slot, None)
            self.on_preview(parents, reports)
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


class MapLookahead:
    """Compare map actions on common determinized futures."""
    boss_reach = {1: 'act1_boss', 2: 'act2_boss', 3: 'act3_second_boss', 4: 'heart'}

    def __init__(self, search, k, seed, limit=600):
        self.search, self.k, self.limit = search, k, limit
        self.rng = torch.Generator().manual_seed(seed)

    def rollout(self, policy, plan, progress):
        search = self.search
        slots = search.take(len(plan))
        parents, actions, seeds = zip(*plan)
        ledgers = {s: deepcopy(search.reward_ledgers[p]) for s, p in zip(slots, parents)}
        acts = {s: progress[p][ACT] for s, p in zip(slots, parents)}
        scores = dict.fromkeys(slots, 0.)
        decisions = 0
        try:
            search.clone(list(parents), slots)
            search.batch.determinize_slots(slots, list(seeds), True)
            search.check(slots)
            live = [s for s in slots if s not in search.failed]
            for slot, key in zip(live, search.loop_keys(live) if live else []):
                search.seen[slot].add(key)
                search.mask_cache.pop(slot, None)
                search.repeat_blocked.pop(slot, None)
            first = dict(zip(slots, actions))
            for tick in range(self.limit):
                if not live:
                    break
                search.clock()
                before = search.progress_for(live)
                if tick == 0:
                    choices = [first[s] for s in live]
                else:
                    mask = search.policy_masks(live)
                    inputs = network_inputs(search.arrays, search.device, torch.tensor(live, device=search.device))
                    inputs['candidate_mask'] &= mask[:, :inputs['candidate_mask'].shape[1]]
                    logits = policy(inputs)[0].masked_fill(~inputs['candidate_mask'], -torch.inf)
                    choices = logits.argmax(-1).tolist()
                search.step(live, choices)
                decisions += len(live)
                after = search.last_step_progress
                remaining = []
                for s in live:
                    if s in search.failed:
                        continue
                    p, ledger = after[s], ledgers[s]
                    scores[s] += ledger.advance(before[s], p)
                    if (p[SCREEN] not in (DEATH, VICTORY) and p[HP] > 0
                            and p[ACT] == acts[s] and not ledger.reach[self.boss_reach[acts[s]]]):
                        remaining.append(s)
                live = remaining
            if live:
                inputs = network_inputs(search.arrays, search.device, torch.tensor(live, device=search.device))
                for slot, value in zip(live, policy(inputs)[1].tolist()):
                    scores[slot] += value
            return [None if s in search.failed else scores[s] for s in slots], decisions
        finally:
            for slot in slots:
                search.failed.pop(slot, None)
                search.mainline_progress.pop(slot, None)
            search.available.extend(slots)

    @torch.no_grad()
    def select(self, policy, parents, candidates, progress):
        plan, ranges = [], []
        for parent, actions in zip(parents, candidates):
            paired = torch.randint(2**63 - 1, (self.k,), generator=self.rng).tolist()
            start = len(plan)
            plan.extend((parent, action, seed) for action in actions for seed in paired)
            ranges.append((start, len(plan)))
        scores, preview_steps = [], 0
        capacity = len(self.search.available)
        for start in range(0, len(plan), capacity):
            result, decisions = self.rollout(policy, plan[start:start + capacity], progress)
            scores.extend(result)
            preview_steps += decisions
        selected, failed = [], 0
        for actions, (start, end) in zip(candidates, ranges):
            trials = scores[start:end]
            if None in trials:
                selected.append(actions[0])
                failed += 1
                continue
            samples = [trials[i:i + self.k] for i in range(0, len(trials), self.k)]
            best = max(range(len(samples)), key=lambda i: statistics.mean(samples[i]))
            differences = [a - b for a, b in zip(samples[best], samples[0])]
            margin = statistics.stdev(differences) / math.sqrt(self.k)
            selected.append(actions[best] if statistics.mean(differences) > margin else actions[0])
        return selected, preview_steps, failed


class MapLookaheadSearch(LookaheadSearch):
    def __init__(self, lanes, device, seed, k=16, limit=600):
        NoSLSearch.__init__(self, lanes, device, seed, storage_slots=max(lanes, k), greedy=True)
        self.lookahead = MapLookahead(self, k, seed, limit)
        self.preview_decisions = self.preview_steps = self.overrides = self.failed_previews = 0

    def record_fault(self, slot, reason, extra_action=None):
        provenance = self.provenance[slot]
        source = provenance[1] if len(provenance) == 3 else slot
        if source >= self.lanes:
            self.failed[source] = reason
        else:
            super().record_fault(slot, reason, extra_action)

    def predict(self, model, slots):
        actions, values, logs = NoSLSearch.predict(self, model, slots)
        progress = self.mainline_progress.copy()
        positions = [i for i, slot in enumerate(slots)
                     if slot not in self.failed and progress[slot][SCREEN] == MAP
                     and progress[slot][ACT] in self.lookahead.boss_reach]
        groups = []
        if positions:
            kinds = self.arrays['candidate_kinds'][[slots[i] for i in positions]]
            routes = self.last_policy_mask[positions, :kinds.shape[1]] & (kinds == 3)
            for i, row in zip(positions, routes.tolist()):
                legal = [j for j, valid in enumerate(row) if valid]
                if len(legal) > 1 and actions[i] in legal:
                    groups.append((i, [actions[i]] + [j for j in legal if j != actions[i]]))
        if groups:
            positions, candidates = zip(*groups)
            selected, steps, failed = self.lookahead.select(model, [slots[i] for i in positions],
                                                            candidates, progress)
            self.preview_decisions += len(positions)
            self.preview_steps += steps
            self.failed_previews += failed
            for i, choice in zip(positions, selected):
                self.overrides += int(actions[i] != choice)
                actions[i] = choice
        return actions, values, logs
