"""Evaluate one policy with combat, card reward and map previews."""
import argparse
from contextlib import contextmanager
from functools import partial
import json
from pathlib import Path
import time

import torch

from train.e4 import BOSS_ROOM, COMBAT_PHASE, ELITE_ROOM, FLOOR, PHASE, ROOM, SCREEN
from train.e4_lookahead import CardRewardLookahead, CombatLookahead, MapLookahead, MapLookaheadSearch
from train.e4_stage12 import CLSPolicy, NoSLCollector, NoSLSearch
from train.model import ModelConfig, SCREENS


class ExamCombatLookahead(CombatLookahead):
    def rollout(self, policy, sources, first, seeds):
        scores, steps = super().rollout(policy, sources, first, seeds)
        self.search.stats['combat']['failed_previews'] += scores.count(None)
        return scores, steps

    def record_preview_fault(self, parent, slot, seed, actions, fault, entities):
        super().record_preview_fault(parent, slot, seed, actions, fault, entities)
        self.search.log_preview_fault('combat', parent, slot, fault)


class SearchExam(MapLookaheadSearch):
    def __init__(self, lanes, device, seed, *, combat_draws=8, combat_options=4,
                 combat_scope='all', override_se=1.0, validate=0, cards=True,
                 map_k=16, key_last_chance=False, extra_storage_slots=0):
        card_options = 5
        storage_slots = max(lanes * combat_draws * combat_options,
                            lanes * card_options * 8 if cards else 0, map_k)
        NoSLSearch.__init__(self, lanes, device, seed, storage_slots=storage_slots + extra_storage_slots,
                            greedy=True, key_last_chance=key_last_chance)
        self.combat_scope = combat_scope
        self.combat_preview = ExamCombatLookahead(
            self, seed, 'combat', on_preview=partial(self.record_previews, 'combat'),
            override_se=override_se, validate=validate, draws=combat_draws, max_options=combat_options)
        self.card_preview = CardRewardLookahead(
            self, seed, max_options=card_options, on_preview=partial(self.record_previews, 'card')) if cards else None
        self.lookahead = MapLookahead(self, map_k, seed) if map_k else None
        self.preview_decisions = self.preview_steps = self.overrides = self.failed_previews = 0
        self.stats = {mode: dict(preview_decisions=0, overrides=0, preview_steps=0, failed_previews=0)
                      for mode in ('combat', 'card', 'map')}
        self.preview_kind = None
        self.preview_faults = []

    def record_previews(self, mode, _parents, reports):
        stats = self.stats[mode]
        stats['preview_decisions'] += sum(report['preview_steps'] > 0 for report in reports)
        stats['preview_steps'] += sum(report['preview_steps'] for report in reports)

    @contextmanager
    def track_previews(self, mode):
        previous = self.preview_kind
        self.preview_kind = mode
        try:
            yield
        finally:
            self.preview_kind = previous

    def log_preview_fault(self, kind, source, slot, fault):
        if len(self.preview_faults) < 20:
            self.preview_faults.append(dict(seed=self.provenance[source][0],
                                            floor=self.progress_for([slot])[slot][FLOOR],
                                            kind=kind, fault=fault))

    def record_fault(self, slot, reason, extra_action=None):
        if slot >= self.lanes and self.preview_kind in ('card', 'map'):
            provenance = self.provenance[slot]
            source = provenance[1] if len(provenance) == 3 else slot
            if source >= self.lanes and source not in self.failed:
                self.stats[self.preview_kind]['failed_previews'] += 1
                code, _, detail = reason.partition(':')
                fault = int(detail) & 0xFFFFFFFF if code == 'engine_fault' else 0
                self.log_preview_fault(self.preview_kind, source, slot, fault)
        super().record_fault(slot, reason, extra_action)

    def apply_previews(self, mode, preview, model, slots, positions, actions):
        if not positions:
            return
        with self.track_previews(mode):
            selected, _ = preview.select(model, [slots[i] for i in positions])
        stats = self.stats[mode]
        for position, action in zip(positions, selected):
            stats['overrides'] += int(actions[position] != action)
            actions[position] = action

    def predict(self, model, slots):
        if self.lookahead is None:
            actions, values, logs = NoSLSearch.predict(self, model, slots)
        else:
            with self.track_previews('map'):
                actions, values, logs = super().predict(model, slots)
            self.stats['map'].update(preview_decisions=self.preview_decisions,
                                     overrides=self.overrides, preview_steps=self.preview_steps)
        progress = self.progress_for(slots)
        combat, card = [], []
        for i, slot in enumerate(slots):
            if slot in self.failed:
                continue
            row = progress[slot]
            if (row[PHASE] == COMBAT_PHASE
                    and (self.combat_scope == 'all' or row[ROOM] in (ELITE_ROOM, BOSS_ROOM))):
                combat.append(i)
            elif self.card_preview is not None and SCREENS[row[SCREEN]] == 'card_reward':
                card.append(i)
        self.apply_previews('combat', self.combat_preview, model, slots, combat, actions)
        self.apply_previews('card', self.card_preview, model, slots, card, actions)
        return actions, values, logs


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--device', default='cpu')
    parser.add_argument('--seed-start', type=int, default=2000000000)
    parser.add_argument('--episodes', type=int, default=256)
    parser.add_argument('--lanes', type=int, default=32)
    parser.add_argument('--combat-draws', type=int, choices=(8, 16, 32), default=8)
    parser.add_argument('--combat-options', type=int, choices=(4, 6), default=4)
    parser.add_argument('--combat-scope', choices=('all', 'elite_boss'), default='all')
    parser.add_argument('--override-se', type=float, default=1.0)
    parser.add_argument('--validate', type=int, default=0)
    parser.add_argument('--cards', choices=('on', 'off'), default='on')
    parser.add_argument('--map-k', type=int, default=16, help='CRN draws per route option. Zero disables map previews.')
    parser.add_argument('--key-last-chance', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    if args.episodes < 1 or args.lanes < 1:
        parser.error('--episodes and --lanes must be positive')
    if args.map_k < 0 or args.map_k == 1:
        parser.error('--map-k must be zero or at least two')
    return args


def evaluate(policy, args, device):
    started = time.monotonic()
    search = SearchExam(min(args.lanes, args.episodes), device, 130, combat_draws=args.combat_draws,
                        combat_options=args.combat_options, combat_scope=args.combat_scope,
                        override_se=args.override_se, validate=args.validate, cards=args.cards == 'on',
                        map_k=args.map_k, key_last_chance=args.key_last_chance)
    collector = NoSLCollector(search, torch.Generator().manual_seed(130))
    seeds = range(args.seed_start, args.seed_start + args.episodes)
    episodes = collector.evaluate(policy, seeds)
    return dict(episodes=episodes, seconds=time.monotonic() - started,
                mainline_decisions=collector.decisions, search=search.stats,
                preview_faults=search.preview_faults,
                combat_draws=args.combat_draws, combat_options=args.combat_options,
                combat_scope=args.combat_scope, override_se=args.override_se, validate=args.validate,
                cards=args.cards, map_k=args.map_k, key_last_chance=args.key_last_chance)


def main():
    args = parse_args()
    torch.set_num_threads(1)
    device = torch.device(args.device)
    policy = CLSPolicy(ModelConfig(384, 6, 6, 1536)).to(device).eval()
    policy.load_state_dict(torch.load(args.model, map_location=device, weights_only=True), strict=True)
    policy.encoder_fp16 = device.type == 'cuda'
    args.output.write_text(json.dumps(evaluate(policy, args, device)) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
