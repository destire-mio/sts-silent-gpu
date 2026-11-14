"""Evaluate one policy with combat, card reward and map previews."""
import argparse
import json
from pathlib import Path
import time

import torch

from train.e4 import BOSS_ROOM, COMBAT_PHASE, ELITE_ROOM, PHASE, ROOM, SCREEN
from train.e4_lookahead import CardRewardLookahead, CombatLookahead, MapLookahead, MapLookaheadSearch
from train.e4_stage12 import CLSPolicy, NoSLCollector, NoSLSearch
from train.model import ModelConfig, SCREENS


class SearchExam(MapLookaheadSearch):
    def __init__(self, lanes, device, seed, *, combat_draws=8, combat_options=4,
                 combat_scope='all', override_se=1.0, validate=0, cards=True,
                 map_k=16, key_last_chance=False):
        card_options = 5
        storage_slots = max(lanes * combat_draws * combat_options,
                            lanes * card_options * 8 if cards else 0, map_k)
        NoSLSearch.__init__(self, lanes, device, seed, storage_slots=storage_slots,
                            greedy=True, key_last_chance=key_last_chance)
        self.combat_scope = combat_scope
        self.combat_preview = CombatLookahead(self, seed, 'combat', override_se=override_se,
                                               validate=validate, draws=combat_draws, max_options=combat_options)
        self.card_preview = CardRewardLookahead(self, seed, max_options=card_options) if cards else None
        self.lookahead = MapLookahead(self, map_k, seed) if map_k else None
        self.preview_decisions = self.preview_steps = self.overrides = self.failed_previews = 0
        self.stats = {mode: dict(preview_decisions=0, overrides=0, preview_steps=0)
                      for mode in ('combat', 'card', 'map')}

    def apply_previews(self, mode, preview, model, slots, positions, actions):
        if not positions:
            return
        selected, reports = preview.select(model, [slots[i] for i in positions])
        stats = self.stats[mode]
        stats['preview_decisions'] += sum(report['preview_steps'] > 0 for report in reports)
        stats['preview_steps'] += sum(report['preview_steps'] for report in reports)
        for position, action in zip(positions, selected):
            stats['overrides'] += int(actions[position] != action)
            actions[position] = action

    def predict(self, model, slots):
        if self.lookahead is None:
            actions, values, logs = NoSLSearch.predict(self, model, slots)
        else:
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
