"""Batched public-action-tree search for combat evaluation."""

from collections import deque
from copy import deepcopy
from dataclasses import dataclass, field
import math
import random

import torch

from train.e4 import COMBAT_PHASE, DEATH, HP, PHASE, SCREEN, VICTORY
from train.model import network_inputs


def public_actions(inputs):
    """Resolve candidate references to visible card, potion, target and choice identities."""
    refs = inputs["candidate_refs"]
    tags, ids = inputs["tags"], inputs["ids"]
    entities = torch.stack((tags[:, 0], tags[:, 1], ids[:, 0], ids[:, 2],
                            inputs["values"][:, 0].long() * (tags[:, 0] == 3)), dim=1)
    indices = (refs.flatten(1) - 1).clamp_min(0)
    gathered = entities.gather(2, indices[:, None].expand(-1, entities.size(1), -1))
    targets = gathered.reshape(len(refs), entities.size(1), 3, -1).permute(0, 3, 2, 1)
    targets = targets.masked_fill(refs.transpose(1, 2)[..., None] == 0, 0)
    return [
        {i: (kind, tuple(map(tuple, target))) for i, (kind, target, legal)
         in enumerate(zip(kinds, references, mask)) if legal}
        for kinds, references, mask in zip(inputs["candidate_kinds"].tolist(), targets.tolist(),
                                           inputs["candidate_mask"].tolist())
    ]


@dataclass
class PublicNode:
    children: dict = field(default_factory=dict)
    visits: int = 0
    pending: int = 0
    total: float = 0.
    expanded: bool = False

    def child(self, action):
        if action not in self.children:
            self.children[action] = PublicNode()
        return self.children[action]

    @property
    def mean(self):
        return self.total / self.visits if self.visits else 0.

    def select(self, actions, priors):
        choices = [(index, self.child(key)) for index, key in actions.items()]
        scale = 2. * math.sqrt(1 + sum(node.visits + node.pending for _, node in choices))
        return max(choices, key=lambda pair: pair[1].mean +
                   scale * priors[pair[0]] / (1 + pair[1].visits + pair[1].pending))


class RootAllocation:
    def __init__(self, slot, actions, logits, sims, rng):
        root = PublicNode()
        self.nodes = {index: root.child(key) for index, key in actions.items()}
        self.argmax = max(actions, key=logits.__getitem__)
        self.rng = rng
        self.live = list(actions)
        # Ensure one initial simulation of every legal action, even at a tiny test budget.
        self.remaining = max(sims, len(actions))
        self.budget = self.remaining
        self.schedule = deque()
        self.gumbel_logits = {
            index: logits[index] - math.log(-math.log(max(rng.random(), 1e-16)))
            for index in actions
        }
        self.slot = slot
        self.failed = 0
        self.steps = 0

    def rank(self, index, max_visits):
        # Gumbel AlphaZero's root ranking with Q in the existing reward/value units.
        return self.gumbel_logits[index] + (50 + max_visits) * self.nodes[index].mean

    def round(self):
        if self.budget != self.remaining:
            max_visits = max(self.nodes[index].visits for index in self.live)
            self.live.sort(key=lambda index: self.rank(index, max_visits), reverse=True)
            self.live = self.live[:(len(self.live) + 1) // 2]
        phases = max(1, (len(self.live) - 1).bit_length())
        per_action = max(1, self.remaining // (phases * len(self.live)))
        count = min(self.remaining, per_action * len(self.live))
        self.schedule.extend(self.live[i % len(self.live)] for i in range(count))
        self.remaining -= count

    def result(self):
        max_visits = max(node.visits for node in self.nodes.values())
        selected = self.argmax
        if max_visits:
            selected = max(self.nodes, key=lambda i: (self.nodes[i].visits, self.rank(i, max_visits)))
        return dict(selected=selected, candidates=list(self.nodes), preview_steps=self.steps,
                    visits={i: n.visits for i, n in self.nodes.items()},
                    attempted=self.budget, failed=self.failed)


@dataclass
class Simulation:
    allocation: RootAllocation
    ledger: object
    path: list
    rollout: bool = False
    steps: int = 0
    reward: float = 0.

    def enter(self, node):
        node.pending += 1
        self.path.append(node)

    def finish(self, score):
        self.allocation.steps += self.steps
        self.allocation.failed += score is None
        for node in self.path:
            node.pending -= 1
            if score is not None:
                node.visits += 1
                node.total += score


class CombatPlanner:
    def __init__(self, search, seed=130, *, sims=64, depth=256, batch_slots=32, on_fault=None):
        if min(sims, depth, batch_slots) < 1:
            raise ValueError("Combat planning requires positive simulation, depth and slot limits.")
        self.search = search
        self.sims = sims
        self.depth = depth
        self.batch_slots = batch_slots
        self.rng = random.Random(seed)
        self.on_fault = on_fault

    def inputs(self, slots):
        return network_inputs(self.search.arrays, self.search.device,
                              torch.as_tensor(slots, device=self.search.device))

    @torch.no_grad()
    def select(self, policy, parents):
        if not parents:
            return [], []
        if any(slot >= self.search.lanes for slot in parents):
            raise ValueError("Combat planning requires live parent lanes.")
        self.search.clock()
        inputs = self.inputs(parents)
        logits, _ = policy(inputs)
        allocations = [
            RootAllocation(slot, actions, row, self.sims, random.Random(self.rng.getrandbits(64)))
            for slot, actions, row in zip(parents, public_actions(inputs), logits.float().tolist())
        ]
        capacity = min(self.batch_slots, len(self.search.available))
        if capacity < 1:
            raise RuntimeError("No combat planning preview slot is available.")
        active = allocations
        while active:
            for root in active:
                root.round()
            pending = deque(active)
            while pending:
                batch = []
                while pending and len(batch) < capacity:
                    root = pending.popleft()
                    batch.append((root, root.schedule.popleft()))
                    if root.schedule:
                        pending.append(root)
                self.simulate(policy, batch)
            active = [root for root in active if root.remaining]
        reports = [root.result() for root in allocations]
        return [report["selected"] for report in reports], reports

    def simulate(self, policy, batch):
        search = self.search
        search.clock()
        slots = search.take(len(batch))
        try:
            parents = [root.slot for root, _ in batch]
            seeds = [root.rng.randrange(1 << 63) for root, _ in batch]
            search.batch.clone_many(parents, slots)
            search.batch.determinize_slots(slots, seeds)
            simulations = {}
            for slot, (root, first) in zip(slots, batch):
                trial = Simulation(root, deepcopy(search.reward_ledgers[root.slot]), [])
                trial.enter(root.nodes[first])
                simulations[slot] = trial
            live = slots
            actions = [first for _, first in batch]
            before = search.progress_for(live)
            finished = {}
            while live:
                search.clock()
                search.batch.step_slots(live, actions)
                faults = torch.stack((search.arrays["faults"].view(torch.int32)[live],
                                      search.arrays["entity_counts"][live],
                                      search.arrays["truncated"][live].int()), dim=1).tolist()
                after = search.progress_for(live)
                continuing = []
                for slot, (fault, entities, truncated) in zip(live, faults):
                    trial = simulations[slot]
                    trial.steps += 1
                    if fault or truncated or not entities:
                        if self.on_fault is not None:
                            self.on_fault(trial.allocation.slot, slot, fault & 0xFFFFFFFF)
                        trial.finish(None)
                        continue
                    progress = after[slot]
                    trial.reward += trial.ledger.advance(before[slot], progress)
                    alive = progress[SCREEN] not in (DEATH, VICTORY) and progress[HP] > 0
                    if alive and progress[PHASE] == COMBAT_PHASE and trial.steps < self.depth:
                        continuing.append(slot)
                    else:
                        finished[slot] = alive
                live, before = continuing, after
                if live:
                    actions = self.next_actions(policy, live, simulations)
            alive = [slot for slot, use_value in finished.items() if use_value]
            values = dict(zip(alive, policy(self.inputs(alive))[1].float().tolist())) if alive else {}
            for slot in finished:
                trial = simulations[slot]
                score = trial.reward + values.get(slot, 0.)
                trial.finish(score if math.isfinite(score) else None)
        finally:
            search.available.extend(slots)

    def next_actions(self, policy, slots, simulations):
        inputs = self.inputs(slots)
        logits, _ = policy(inputs)
        logits = logits.float().masked_fill(~inputs["candidate_mask"], -torch.inf)
        argmax = logits.argmax(-1).tolist()
        tree_rows = []
        for row, slot in enumerate(slots):
            trial = simulations[slot]
            if trial.rollout:
                continue
            node = trial.path[-1]
            if node.expanded:
                tree_rows.append(row)
            else:
                node.expanded = trial.rollout = True
        if tree_rows:
            actions = public_actions({name: value[tree_rows] for name, value in inputs.items()})
            priors = logits[tree_rows].softmax(-1).tolist()
            for row, legal, prob in zip(tree_rows, actions, priors):
                trial = simulations[slots[row]]
                argmax[row], node = trial.path[-1].select(legal, prob)
                trial.enter(node)
        return argmax

