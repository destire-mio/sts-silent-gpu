from copy import deepcopy

import torch

from train.model import store_inputs
from train.e4_stage6.combat import combat_gae, retain_trials


class CombatTape:
    def __init__(self, rng):
        self.rng = rng
        self.groups = []
        self.native_replay_decisions = 0

    def record(self, search, context, slots, actions, logs, values):
        masks = search.policy_masks(slots).cpu()
        for j, slot in enumerate(slots):
            context['copy_rows'][slot].append(dict(
                policy_mask=masks[j].clone(), action=actions[j], log=logs[j], value=values[j]))

    def replay_retained(self, search, context, slots, selected, parent, winner):
        targets = [slot for i, slot in enumerate(slots) if i not in selected and slot != winner][:8]
        sources = [slots[i] for i in selected]
        original_states = [search.batch.state_bytes(slot) for slot in sources]
        actor_rng = search.rng.get_state().clone()
        search.clone([parent] * 8, targets)
        histories = {target: context['copy_rows'][source] for target, source in zip(targets, sources)}
        positions = dict.fromkeys(targets, 0)
        live = targets[:]
        while live:
            search.clock()
            index = torch.tensor(live, dtype=torch.int64, device=search.device)
            lengths = (4 + 24 * search.arrays['entity_counts'][index].long()
                       + 7 * search.arrays['candidate_counts'][index].long())
            offsets = torch.zeros(len(live) + 1, dtype=torch.int64, device=search.device)
            data = torch.empty(int(lengths.sum()), dtype=torch.uint8, device=search.device)
            store_inputs(search.batch, search.arrays, index, data, offsets)
            data, offsets = data.cpu(), offsets.cpu().tolist()
            actions = []
            for j, target in enumerate(live):
                row = histories[target][positions[target]]
                row['data'] = data[offsets[j]:offsets[j + 1]].clone()
                actions.append(row['action'])
                positions[target] += 1
            search.step(live, actions)
            self.native_replay_decisions += len(live)
            live = [target for target in live if positions[target] < len(histories[target])]
        for target, expected in zip(targets, original_states):
            assert search.batch.state_bytes(target) == expected, 'Retained replay diverged from original sampled copy'
        assert torch.equal(actor_rng, search.rng.get_state()), 'Observation replay consumed actor sampling RNG'

    def finish(self, search, context, slots, scores, winner, parent):
        assert len(slots) == 32 and len(scores) == 32
        score = torch.tensor([scores[s] for s in slots], dtype=torch.float32)
        assert bool(torch.isfinite(score).all()) and bool(((score >= 0) & (score <= 1)).all())
        selected = retain_trials(self.rng).tolist()
        self.replay_retained(search, context, slots, selected, parent, winner)
        mean = float(score.mean())
        trials = []
        for i in selected:
            rows = context['copy_rows'][slots[i]]
            assert rows
            values = torch.tensor([r['value'] for r in rows], dtype=torch.float32)
            advantage, target = combat_gae(values, float(score[i]), mean)
            trials.append(dict(trial=i, adopted=slots[i] == winner, rows=rows,
                               advantages=advantage, returns=target))
        self.groups.append(dict(seed=search.provenance[parent][0],
            prefix=list(search.provenance[parent][1]), scores=score,
            group_mean=mean, adopted_trial=slots.index(winner), selected_trials=selected, trials=trials))

    def state(self):
        return dict(rng=self.rng.get_state().clone(), groups=deepcopy(self.groups),
                    native_replay_decisions=self.native_replay_decisions)

    def restore(self, state):
        self.rng.set_state(state['rng'])
        self.groups = deepcopy(state['groups'])
        self.native_replay_decisions = state['native_replay_decisions']

    def drain(self):
        groups = self.groups
        self.groups = []
        return groups


def pack_trials(groups, device):
    rows, advantages, returns, identities, training_seeds = [], [], [], [], []
    for group_id, group in enumerate(groups):
        for trial in group['trials']:
            rows.extend(trial['rows'])
            advantages.append(trial['advantages'])
            returns.append(trial['returns'])
            identities.extend([(group_id, trial['trial'], t) for t in range(len(trial['rows']))])
            training_seeds.extend([group['seed']] * len(trial['rows']))
    assert rows
    sizes = torch.tensor([len(r['data']) for r in rows], dtype=torch.int64, device=device)
    offsets = torch.cat((torch.zeros(1, dtype=torch.int64, device=device), sizes.cumsum(0)))
    data = torch.empty(sum(len(r['data']) for r in rows), dtype=torch.uint8, device=device)
    position = 0
    for group in groups:
        for trial in group['trials']:
            chunk = torch.cat([r['data'] for r in trial['rows']])
            data[position:position+len(chunk)].copy_(chunk)
            position += len(chunk)
    assert position == len(data)
    return dict(data=data, offsets=offsets,
                actions=torch.tensor([r['action'] for r in rows], dtype=torch.int64, device=device),
                logs=torch.tensor([r['log'] for r in rows], dtype=torch.float32, device=device),
                values=torch.tensor([r['value'] for r in rows], dtype=torch.float32, device=device),
                policy_mask=torch.stack([r['policy_mask'] for r in rows]).to(device),
                advantages=torch.cat(advantages).to(device), returns=torch.cat(returns).to(device), identities=identities,
                training_seeds=torch.tensor(training_seeds, dtype=torch.int64, device=device))
