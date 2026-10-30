import statistics
import time

import torch

from train.e4 import COMBAT_PHASE, PHASE, SCREEN, DEATH, VICTORY, BATTLE_OVER, SMOKED, HP, MAX_REWARD
from train.model import FIELDS, network_inputs


class CombatSolver:
    def __init__(self, search, policy, rng):
        self.search, self.policy, self.rng = search, policy, rng
        self.device = search.device
        self.progress_buffer = torch.empty(search.size * 9, dtype=torch.int64, device=self.device)
        self.selected = torch.zeros(search.size, dtype=torch.int32, device=self.device)
        self.active = torch.zeros(search.size, dtype=torch.uint8, device=self.device)
        self.predict_calls = self.step_calls = self.progress_reads = 0

    def progress(self, slots, actions, values=None, branched=None):
        self.search.batch.progress_tensors(self.progress_buffer)
        index = torch.tensor(slots, device=self.device, dtype=torch.int64)
        columns = [actions[:, None], self.search.arrays['faults'][index, None],
                   self.progress_buffer.view(-1, 9)[index]]
        if values is not None:
            columns.extend((values[:, None], branched[:, None]))
        rows = torch.cat([column.double() for column in columns], dim=1).cpu().tolist()
        self.progress_reads += 1
        assert not any(row[1] for row in rows), 'SL solver engine fault'
        return rows

    def inputs(self, slots):
        index = torch.tensor(slots, device=self.device, dtype=torch.int64)
        return network_inputs({k: self.search.arrays[k] for k in FIELDS}, self.device, index)

    def step(self, slots, actions):
        self.selected.zero_()
        self.active.zero_()
        self.selected[slots] = actions.to(torch.int32)
        self.active[slots] = 1
        self.search.batch.step_tensors(self.selected, self.active)
        self.step_calls += 1

    @torch.no_grad()
    def rollouts(self, slots, prefixes, mutations):
        paths = [[] for _ in slots]
        branch_points = [[] for _ in slots]
        values = [[] for _ in slots]
        live = list(range(len(slots)))
        final = [None] * len(slots)
        while live:
            self.search.clock()
            current = [slots[j] for j in live]
            inputs = self.inputs(current)
            logits, run_values, _ = self.policy(inputs)
            self.predict_calls += 1
            actions = torch.multinomial(logits.softmax(-1), 1, generator=self.rng).flatten()
            branched = inputs['candidate_mask'].sum(-1) > 1
            forced, changed, originals = [], [], []
            for i, j in enumerate(live):
                t = len(paths[j])
                if t < len(prefixes[j]):
                    forced.append((i, prefixes[j][t]))
                elif t == len(prefixes[j]) and mutations[j] is not None:
                    changed.append(i)
                    originals.append(mutations[j])
            if forced:
                actions[[i for i, _ in forced]] = torch.tensor([a for _, a in forced], device=self.device)
            if changed:
                ranked = torch.argsort(logits[changed], descending=True, stable=True)[:, :8]
                allowed = inputs['candidate_mask'][changed].gather(1, ranked)
                allowed &= ranked != torch.tensor(originals, device=self.device)[:, None]
                weights = logits[changed].gather(1, ranked).masked_fill(~allowed, -torch.inf).softmax(-1)
                picks = torch.multinomial(weights, 1, generator=self.rng)
                actions[changed] = ranked.gather(1, picks).flatten()
            self.step(current, actions)
            rows = self.progress(current, actions, run_values, branched)
            continuing = []
            for j, row in zip(live, rows):
                paths[j].append(int(row[0]))
                values[j].append(row[11])
                if row[12]:
                    branch_points[j].append(len(paths[j]) - 1)
                p = [int(v) for v in row[2:11]]
                if p[PHASE] != COMBAT_PHASE or p[SCREEN] in (DEATH, VICTORY):
                    final[j] = p
                else:
                    assert len(paths[j]) < 3000, 'SL solver rollout exceeded 3000 decisions'
                    continuing.append(j)
            live = continuing
        _, score_values, _ = self.policy(self.inputs(slots))
        scores = score_values.cpu().tolist()
        return [dict(actions=path, branch_points=points, values=predictions, progress=p,
                     score=0. if p[SCREEN] == DEATH else score)
                for path, points, predictions, p, score in zip(paths, branch_points, values, final, scores)]

    def endpoint_rows(self, slots):
        inputs = self.inputs(slots)
        player = inputs['tags'][:, 0] == 2
        hp = (inputs['values'][:, 0] * player).sum(-1)
        max_hp = (inputs['values'][:, 1] * player).sum(-1)
        potion = ((inputs['tags'][:, 0] == 7) & (inputs['tags'][:, 1] == 0)
                  & (inputs['ids'][:, 0] != 1) & inputs['entity_mask']).sum(-1)
        return torch.stack((hp / max_hp, potion), dim=1).cpu().tolist()

    @torch.no_grad()
    def replay(self, parents, expected_slots, records):
        live = list(range(len(parents)))
        step = 0
        while live:
            self.search.clock()
            current = [parents[j] for j in live]
            actions = torch.tensor([records[j]['actions'][step] for j in live], device=self.device)
            self.step(current, actions)
            rows = self.progress(current, actions)
            continuing = []
            for j, row in zip(live, rows):
                p = [int(v) for v in row[2:]]
                if step + 1 < len(records[j]['actions']):
                    assert p[PHASE] == COMBAT_PHASE and p[SCREEN] not in (DEATH, VICTORY)
                    continuing.append(j)
                else:
                    assert p == records[j]['progress'], 'SL real-versus-clone progress mismatch'
            step += 1
            live = continuing
        pair = self.inputs(expected_slots + parents)
        n = len(parents)
        for tensor in pair.values():
            assert torch.equal(tensor[:n], tensor[n:]), 'SL real-versus-clone observation mismatch'
        _, scores, _ = self.policy(pair)
        expected, actual = scores[:n].cpu().tolist(), scores[n:].cpu().tolist()
        for record, left, right in zip(records, expected, actual):
            assert left == right, 'SL real-versus-clone score mismatch'
            record['replay_score'] = 0. if record['progress'][SCREEN] == DEATH else right
            record['exact_replay'] = True

    @torch.no_grad()
    def solve(self, parents):
        started = time.monotonic()
        n = len(parents)
        slots = self.search.take(n * 34)
        trials, best_slots, initial_slots = slots[:n * 32], slots[n * 32:n * 33], slots[n * 33:]
        self.search.batch.clone_many([p for p in parents for _ in range(32)], trials)
        initial = self.rollouts(trials, [[] for _ in trials], [None] * len(trials))
        winners = [max(range(j * 32, (j + 1) * 32), key=lambda k:initial[k]['score']) for j in range(n)]
        best = [dict(initial[k]) for k in winners]
        n32 = [dict(record) for record in best]
        single = [dict(initial[j * 32]) for j in range(n)]
        single_ends = self.endpoint_rows([trials[j * 32] for j in range(n)])
        self.search.batch.clone_many([trials[k] for k in winners], best_slots)
        self.search.batch.clone_many(best_slots, initial_slots)
        counts, misses, rounds = [32] * n, [0] * n, [0] * n
        improving = [j for j in range(n) if best[j]['branch_points']]
        while improving:
            self.search.clock()
            owners = [j for j in improving for _ in range(min(16, 4096 - counts[j]))]
            current = trials[:len(owners)]
            self.search.batch.clone_many([parents[j] for j in owners], current)
            points = [best[j]['branch_points'] for j in owners]
            draws = torch.rand(len(owners), generator=self.rng, device=self.device).cpu().tolist()
            times = [p[int(u * len(p))] for p, u in zip(points, draws)]
            prefixes = [best[j]['actions'][:t] for j, t in zip(owners, times)]
            mutations = [best[j]['actions'][t] for j, t in zip(owners, times)]
            results = self.rollouts(current, prefixes, mutations)
            updates = []
            for j in improving:
                group = [k for k, owner in enumerate(owners) if owner == j]
                winner = max(group, key=lambda k:results[k]['score'])
                counts[j] += len(group)
                rounds[j] += 1
                if results[winner]['score'] > best[j]['score']:
                    best[j] = results[winner]
                    misses[j] = 0
                    updates.append((current[winner], best_slots[j]))
                else:
                    misses[j] += 1
            if updates:
                self.search.batch.clone_many([s for s, _ in updates], [t for _, t in updates])
            improving = [j for j in improving if misses[j] < 8 and counts[j] < 4096
                         and best[j]['branch_points']]
        self.replay(parents, best_slots, best)
        for records, ends in ((best, self.endpoint_rows(parents)), (n32, self.endpoint_rows(initial_slots)),
                              (single, single_ends)):
            for j, (record, (hp_fraction, potions)) in enumerate(zip(records, ends)):
                p = record['progress']
                record.update(death=p[SCREEN] == DEATH, won=bool(p[BATTLE_OVER] and not p[SMOKED] and p[HP] > 0),
                    escaped=bool(p[SMOKED]), hp_fraction=hp_fraction, potions_remaining=int(potions),
                    trials=counts[j] if records is best else 32 if records is n32 else 1,
                    rounds=rounds[j] if records is best else 0,
                    no_improvement_rounds=misses[j] if records is best else 0)
        for parent, record in zip(parents, best):
            self.search.provenance[parent][1].extend(record['actions'])
        self.search.available.extend(slots)
        return dict(S=best, N32=n32, B2=single, batch_seconds=time.monotonic() - started,
                    predict_calls=self.predict_calls, step_calls=self.step_calls, progress_reads=self.progress_reads)


def summarize(rows):
    result = {}
    for group in sorted({r['group'] for r in rows}):
        selected = [r for r in rows if r['group'] == group]
        wins = [r for r in selected if r['won']]
        n = len(selected)
        result[group] = dict(scenes=n, deaths=sum(r['death'] for r in selected),
            death_rate=sum(r['death'] for r in selected) / n,
            mean_score=sum(r['score'] for r in selected) / n,
            trials_median=statistics.median(r['trials'] for r in selected),
            trials_max=max(r['trials'] for r in selected),
            mean_hp_fraction=sum(r['hp_fraction'] for r in selected) / n,
            mean_potions_remaining=sum(r['potions_remaining'] for r in selected) / n,
            won_scenes=len(wins), won_mean_hp_fraction=
                sum(r['hp_fraction'] for r in wins) / len(wins) if wins else None,
            won_mean_potions_remaining=sum(r['potions_remaining'] for r in wins) / len(wins) if wins else None)
    return result


def value_calibration(predictions, returns):
    assert len(predictions) == len(returns) and predictions
    bins = [[] for _ in range(10)]
    for prediction, actual in zip(predictions, returns):
        bins[min(9, int(prediction * 10))].append((prediction, actual))
    mean = statistics.mean(returns)
    variance = statistics.mean((y - mean) ** 2 for y in returns)
    errors = [y - p for p, y in zip(predictions, returns)]
    error_mean = statistics.mean(errors)
    error_variance = statistics.mean((e - error_mean) ** 2 for e in errors)
    return dict(samples=len(predictions), bins=[dict(lower=i / 10, upper=(i + 1) / 10, samples=len(b),
        mean_prediction=statistics.mean(p for p, _ in b) if b else None,
        mean_actual_remaining_reward=statistics.mean(y for _, y in b) if b else None)
        for i, b in enumerate(bins)], explained_variance=1 - error_variance / variance if variance else None)


def examination_calibration(scenes, rows, episodes):
    baseline = {episode['seed']: episode for episode in episodes}
    predictions, returns = [], []
    for scene, row in zip(scenes, rows):
        episode = baseline[scene['seed']]
        assert episode['candidates'][:len(scene['prefix'])] == scene['prefix']
        assert scene['act'] in (1, 2)
        earned = scene['floor'] + 5 * (scene['act'] - 1)
        predictions.append(row['values'][0])
        returns.append((round(episode['reward'] * MAX_REWARD) - earned) / MAX_REWARD)
    report = value_calibration(predictions, returns)
    report.update(prediction_scope='B2 run-value at the exact examination root',
        actual_return_scope='Realized remaining reward in the existing B4145 N32 evaluation episode',
        source_job_id=4145, additional_rollouts=0,
        S_counterfactual_full_run_returns_observed=False)
    return report
