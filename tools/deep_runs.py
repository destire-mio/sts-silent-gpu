import argparse
import fcntl
import json
import random
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import spire

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'oracle'))
from record import run_jobs
from replay import replay
from check_oracle_frames import check_recordings


CARD_VALUE = {
    'Adrenaline': 15, 'After Image': 14, 'Wraith Form v2': 15, 'Footwork': 14,
    'Noxious Fumes': 13, 'Corpse Explosion': 15, 'Catalyst': 11, 'Burst': 11,
    'Backflip': 10, 'Leg Sweep': 12, 'Dash': 11, 'Predator': 11, 'Glass Knife': 13,
    'Die Die Die': 12, 'Blade Dance': 12, 'Cloak And Dagger': 10, 'Deadly Poison': 10,
    'Crippling Poison': 12, 'Bouncing Flask': 12, 'PiercingWail': 12,
    'Well Laid Plans': 12, 'Calculated Gamble': 10, 'Escape Plan': 10,
    'Dagger Spray': 10, 'Dagger Throw': 9, 'Flying Knee': 9, 'Quick Slash': 8,
    'Acrobatics': 9, 'Deflect': 8, 'Blur': 9, 'Dodge and Roll': 9, 'Terror': 12,
    'Neutralize': 8, 'Survivor': 7, 'Accuracy': 9, 'Backstab': 10,
    'Malaise': 12, 'Venomology': 14, 'Thousand Cuts': 9, 'A Thousand Cuts': 9,
    'Bullet Time': 10, 'Tools of the Trade': 8, 'Doppelganger': 5,
    'Finisher': 5, 'Heel Hook': 6, 'Choke': 6, 'Endless Agony': 7,
    'Poisoned Stab': 9, 'Skewer': 10, 'Expertise': 7, 'Setup': 1,
    'Concentrate': 2, 'Prepared': 3, 'Distraction': 3, 'Reflex': 2, 'Tactician': 2,
    'Grand Finale': 0, 'Slice': 5, 'Unload': 6, 'Storm of Steel': 4,
    'Strike_G': 2, 'Defend_G': 3, 'AscendersBane': -6,
    'Regret': -12, 'Injury': -12, 'Normality': -12, 'Pain': -12, 'Shame': -12,
    'Doubt': -12, 'Parasite': -12, 'Decay': -12, 'Writhe': -12, 'Clumsy': -12,
    'Necronomicurse': -12, 'CurseOfTheBell': -12, 'Pride': -12,
    'Dazed': -12, 'Wound': -12, 'Burn': -12, 'Slimed': -8, 'Void': -12,
}


def fields(snapshot, ref):
    return snapshot['objects'][ref['ref']]['fields']


def deck(snapshot):
    player = fields(snapshot, snapshot['player'])
    group = fields(snapshot, player['AbstractPlayer.masterDeck'])
    return [fields(snapshot, ref) for ref in group['CardGroup.group']]


def card_value(card_id):
    return CARD_VALUE.get(card_id, 5)


def pick(rng, actions, weights):
    return rng.choices(actions, weights=weights)[0]


def combat_action(game, state, rng, actions):
    if any(action['type'] == 'choose' for action in actions):
        weights = []
        for action in actions:
            if action['type'] == 'choose':
                value = card_value(state['choices'][action['option']])
                weights.append(max(1, 16 - value if state['remove'] else value + 2))
            else:
                weights.append(1)
        return pick(rng, actions, weights)
    incoming = sum(max(0, monster['damage']) * max(1, monster['hits'])
                   for monster in state['monsters'] if monster['alive'])
    weights = []
    for action in actions:
        kind = action['type']
        if kind == 'play':
            card = state['hand'][action['card']]
            damage = max(0, card['damage'])
            block = min(max(0, card['block']), max(0, incoming - state['block']))
            value = 2 + damage + 1.3 * block
            if card['type'] == 2:
                value += 16 if state['turn'] < 3 else 5
            if card['id'] in ('Deadly Poison', 'Bouncing Flask', 'Crippling Poison', 'Corpse Explosion',
                              'Adrenaline', 'Backflip', 'Acrobatics', 'Terror', 'Neutralize'):
                value += 12
            if action['target'] >= 0:
                monster = state['monsters'][action['target']]
                if damage >= monster['hp'] + monster['block']:
                    value += 25
                value += 8 / max(1, monster['hp'])
            if card['id'] == 'Wraith Form v2' and state['turn'] < 4 and sum(
                    monster['hp'] for monster in state['monsters'] if monster['alive']) > 80:
                value = 0.01
            weights.append(max(1, value / max(1, card['cost'])))
        elif kind == 'end_turn':
            weights.append(0.05 if any(a['type'] == 'play' for a in actions) else 5)
        elif kind == 'potion_use':
            weights.append(8 if state['hp'] < incoming + 20 or state['floor'] % 17 >= 15 else 0.2)
        elif kind == 'potion_discard':
            weights.append(0.001)
        else:
            weights.append(1)
    return pick(rng, actions, weights)


def outside_action(game, rng):
    snapshot = game.snapshot()
    screen = snapshot['screen']
    state = game.search_state()
    actions = game.legal_actions()
    cards = deck(snapshot)
    if state['act'] == 3 and snapshot['node'][1] == 14 and not state['keys'][0] and screen['choice'] == 'REST':
        return next(action for action in actions if action['type'] == 'choose'
                    and screen['options'][action['option']] == 'RecallOption')
    weights = []
    for action in actions:
        kind = action['type']
        if kind != 'choose':
            weights.append(2 if kind == 'skip' else 1)
            continue
        option = screen['options'][action['option']]
        weight = 5
        if screen['type'] == 'CARD_REWARD':
            weight = max(0.1, card_value(option) - (5 if len(cards) > 20 else 2)) ** 2
        elif screen['type'] == 'GRID':
            selection = fields(snapshot, screen['state'])
            value = card_value(option)
            weight = max(1, 16 - value if selection['GridCardSelectScreen.forPurge']
                         or selection['GridCardSelectScreen.forTransform'] else value) ** 2
        elif screen['choice'] == 'REST':
            weight = {'RestOption': 30 if state['hp'] < state['max_hp'] * 0.7 else 2,
                      'SmithOption': 15, 'RecallOption': 40 if state['act'] == 3 else 4}.get(option, 5)
        elif screen['type'] == 'MAP' and option.startswith('x='):
            x = int(option[2:])
            y = 0 if not snapshot['run']['firstRoomChosen'] else snapshot['node'][1] + 1
            node = snapshot['map'][y][x]
            room = node['room']
            weight = {'RestRoom': 20, 'EventRoom': 16, 'MonsterRoom': 10,
                      'ShopRoom': 8, 'MonsterRoomElite': 2}.get(room, 10)
            if node['emerald'] and not state['keys'][1]:
                weight = 20
        elif option == 'SAPPHIRE_KEY':
            weight = 50
        elif screen['type'] == 'SHOP':
            weight = max(0.1, card_value(option) - 5) ** 2 if option in CARD_VALUE else 8
            if option == 'PrismaticShard':
                weight = 0.01
            if option == 'purge':
                weight = 30
        elif screen['type'] == 'BOSS_REWARD':
            weight = {'Runic Pyramid': 30, 'Snecko Eye': 18, 'Cursed Key': 24,
                      'Sozu': 16, 'Coffee Dripper': 6, 'Fusion Hammer': 20,
                      'Busted Crown': 5, 'Ectoplasm': 8, 'Velvet Choker': 5,
                      'Runic Dome': 20, 'Astrolabe': 16, 'Pandora\'s Box': 25}.get(option, 10)
        weights.append(weight)
    return pick(rng, actions, weights)


def combat_score(game):
    state = game.search_state()
    enemies = [monster for monster in state['monsters'] if monster['alive']]
    poison = sum(amount for monster in enemies for name, amount in monster['powers'] if name == 'Poison')
    scaling = sum(amount * {'Dexterity': 20, 'Strength': 10, 'Noxious Fumes': 20,
                            'After Image': 30, 'Accuracy': 12, 'Retain Cards': 15}.get(name, 0)
                  for name, amount in state['powers'])
    return state['hp'] * 2 - sum(monster['hp'] + monster['block'] * 0.2 for monster in enemies) * 1.5 \
        - len(enemies) * 80 + poison * 2.5 + scaling


def fight(game, rng, prefix):
    beam = [(game, [])]
    finished = []
    for _ in range(60):
        children = []
        for parent, previous in beam:
            turn = parent.search_state()['turn']
            for _ in range(12):
                trial = parent.copy()
                actions = []
                state = trial.search_state()
                for _ in range(120):
                    if state['card_slots'] >= 128:
                        break
                    legal = trial.legal_actions()
                    assert legal, json.dumps(dict(seed=trial.snapshot()['run']['seed'],
                        actions=prefix + previous + actions, snapshot=trial.snapshot()))
                    action = combat_action(trial, state, rng, legal)
                    trial.step(action)
                    actions.append(action)
                    state = trial.search_state()
                    if state['dead'] or not state['combat'] or state['turn'] != turn:
                        break
                assert state['dead'] or not state['combat'] or state['turn'] != turn or state['card_slots'] >= 128, state
                candidate = trial, previous + actions
                if state['dead'] or not state['combat']:
                    finished.append(candidate)
                elif state['card_slots'] < 128:
                    children.append(candidate)
                else:
                    print(f'prune card budget seed={trial.snapshot()["run"]["seed"]} '
                          f'floor={state["floor"]} slots={state["card_slots"]}', flush=True)
        if not children:
            break
        children.sort(key=lambda candidate: combat_score(candidate[0]), reverse=True)
        seen = set()
        beam = []
        remaining = []
        for candidate in children:
            key = json.dumps(candidate[0].search_state(), sort_keys=True)
            if key not in seen:
                seen.add(key)
                beam.append(candidate)
            else:
                remaining.append(candidate)
            if len(beam) == 6:
                break
        beam.extend(remaining[:6 - len(beam)])
        wins = [candidate for candidate in finished if not candidate[0].search_state()['dead']]
        if wins and max(candidate[0].search_state()['hp'] for candidate in wins) >= max(
                candidate[0].search_state()['hp'] for candidate in beam):
            break
    if not finished:
        return None
    return max(finished, key=lambda candidate: (not candidate[0].search_state()['dead'],
                                              candidate[0].search_state()['hp'], -len(candidate[1])))


@dataclass
class Candidate:
    game: object
    actions: list


def floor_score(candidate):
    state = candidate.game.search_state()
    snapshot = candidate.game.snapshot()
    cards = deck(snapshot)
    quality = sum(card_value(card['AbstractCard.cardID'])
                  + 2 * card['AbstractCard.timesUpgraded'] - 4 for card in cards)
    quality -= max(0, len(cards) - 25) * 4
    player = fields(snapshot, snapshot['player'])
    return state['hp'] + 2 * quality + 8 * len(player['AbstractPlayer.relics']) + 32 * sum(state['keys'])


def find(seed):
    rng = random.Random(seed)
    game = spire.reset(seed)
    beam = [Candidate(game, [])]
    terminal = []
    frontier = []
    for _ in range(256):
        children = []
        for parent in beam:
            floor = parent.game.search_state()['floor']
            base = parent.game
            prefix = []
            if base.search_state()['combat']:
                outcome = fight(base, rng, parent.actions)
                if outcome is None:
                    continue
                base, prefix = outcome
            for _ in range(32 if floor == 0 else 4):
                trial = base.copy()
                actions = list(prefix)
                while True:
                    state = trial.search_state()
                    if state['dead'] or state['victory']:
                        terminal.append(Candidate(trial, parent.actions + actions))
                        break
                    if state['floor'] > floor:
                        children.append(Candidate(trial, parent.actions + actions))
                        break
                    if state['combat']:
                        outcome = fight(trial, rng, parent.actions + actions)
                        if outcome is None:
                            break
                        trial, inputs = outcome
                        actions.extend(inputs)
                    else:
                        assert trial.legal_actions(), json.dumps(dict(seed=seed,
                            actions=parent.actions + actions, snapshot=trial.snapshot()))
                        action = outside_action(trial, rng)
                        trial.step(action)
                        actions.append(action)
        if not children:
            if any(c.game.search_state()['act'] >= 3 and c.game.search_state()['floor'] >= 50 for c in terminal):
                break
            if not frontier:
                break
            beam = frontier.pop()
            continue
        children.sort(key=floor_score, reverse=True)
        seen = set()
        builds = set()
        beam = []
        remaining = []
        for child in children:
            key = json.dumps(child.game.snapshot(), sort_keys=True)
            if key not in seen:
                seen.add(key)
                snapshot = child.game.snapshot()
                player = fields(snapshot, snapshot['player'])
                build = tuple((card['AbstractCard.cardID'], card['AbstractCard.timesUpgraded'])
                              for card in deck(snapshot)), tuple(
                    fields(snapshot, ref)['AbstractRelic.relicId'] for ref in player['AbstractPlayer.relics'])
                if build not in builds:
                    builds.add(build)
                    beam.append(child)
                else:
                    remaining.append(child)
            if len(beam) == 8:
                break
        beam.extend(remaining[:8 - len(beam)])
        selected = {json.dumps(child.game.snapshot(), sort_keys=True) for child in beam}
        alternatives = []
        for child in children:
            key = json.dumps(child.game.snapshot(), sort_keys=True)
            if key not in selected:
                selected.add(key)
                alternatives.append(child)
            if len(alternatives) == 8:
                break
        if alternatives:
            frontier.append(alternatives)
        print(f'seed={seed} floor={beam[0].game.search_state()["floor"]} '
              f'candidates={len(beam)} hp={beam[0].game.search_state()["hp"]}', flush=True)
    if not terminal:
        return None
    return max(terminal, key=lambda c: (c.game.search_state()['act'], c.game.search_state()['floor'],
                                      c.game.search_state()['victory'],
                                      c.game.search_state()['hp']))


def main():
    parser = argparse.ArgumentParser(description='Find and record deterministic whole games from Neow')
    parser.add_argument('seed', type=int)
    parser.add_argument('count', type=int)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    jobs = []
    predictions = []
    seed = args.seed
    while len(jobs) < args.count:
        start = time.monotonic()
        opening = spire.reset(seed)
        opening.step({'type': 'choose', 'option': 0})
        opening.step({'type': 'choose', 'option': 3})
        snapshot = opening.snapshot()
        player = fields(snapshot, snapshot['player'])
        if not any(fields(snapshot, ref)['AbstractRelic.relicId'] == "Pandora's Box"
                   for ref in player['AbstractPlayer.relics']):
            predictions.append(dict(seed=seed, opening_rejected=True, selected=False,
                                    search_seconds=time.monotonic() - start))
            (args.output / 'predictions.json').write_text(json.dumps(predictions, indent=2))
            seed += 1
            continue
        candidate = find(seed)
        if candidate is None:
            predictions.append(dict(seed=seed, selected=False, search_cutoff=True,
                                    search_seconds=time.monotonic() - start))
            (args.output / 'predictions.json').write_text(json.dumps(predictions, indent=2))
            seed += 1
            continue
        state = candidate.game.search_state()
        seconds = time.monotonic() - start
        scenario = {'actions': candidate.actions}
        (args.output / f'{seed}.scenario.json').write_text(json.dumps(scenario))
        selected = state['act'] >= 3 and state['floor'] >= 50
        if selected:
            jobs.append(dict(seed=seed, scenario=scenario, output=str(args.output.resolve() / f'{seed}.jsonl')))
        predictions.append(dict(seed=seed, act=state['act'], floor=state['floor'],
                                victory=state['victory'], selected=selected,
                                decisions=len(candidate.actions), search_seconds=seconds))
        print(json.dumps(predictions[-1]), flush=True)
        (args.output / 'jobs.json').write_text(json.dumps(jobs))
        (args.output / 'predictions.json').write_text(json.dumps(predictions, indent=2))
        seed += 1
    with (ROOT / 'oracle/instance/deep-runs.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        recording = run_jobs(jobs)
    results = []
    for job in jobs:
        result = replay(job['output'], spire)
        with Path(job['output']).open() as stream:
            next(stream)
            rows = [json.loads(line) for line in stream]
        snapshots = [row['snapshot'] for row in rows]
        reached = dict(act2=any(s['run']['actNum'] >= 2 for s in snapshots),
                       act3=any(s['run']['actNum'] >= 3 for s in snapshots),
                       act3_boss=any(s['run']['actNum'] == 3 and s['objects'][s['room']['ref']]['class']
                                     == 'rooms.MonsterRoomBoss' for s in snapshots),
                       act4=any(s['run']['actNum'] == 4 for s in snapshots),
                       heart=any(obj['class'] == 'monsters.ending.CorruptHeart'
                                 for s in snapshots for obj in s['objects']),
                       win=rows[-1]['terminal'] == 'victory' and snapshots[-1]['run']['actNum'] == 4)
        results.append(dict(seed=job['seed'], depth=reached, decisions=result.decisions,
                            scopes={name: dict(passed=status.passed, decisions=status.matched_decisions,
                                              terminal=status.terminal_matches, differences=status.differences)
                                    for name, status in result.scopes.items()}, stopped=result.stopped))
        print(json.dumps(results[-1]), flush=True)
    summary = dict(depth={name: sum(row['depth'][name] for row in results) for name in reached},
                   recording=recording, results=results)
    (args.output / 'replay.json').write_text(json.dumps(summary, indent=2))
    frames = args.output / 'idle-frames'
    frames.mkdir()
    with (ROOT / 'oracle/instance/deep-runs.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        check_recordings(frames, jobs[:10])
    return int(any(not row['scopes'][name]['passed'] for row in results for name in ('combat', 'run')))


if __name__ == '__main__':
    raise SystemExit(main())
