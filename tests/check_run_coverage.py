"""Audit observable run behavior in original-game recordings."""
import argparse
import gzip
import json
from pathlib import Path


def fields(snapshot, reference):
    return snapshot['objects'][reference['ref']]['fields']


def evidence(path):
    with (gzip.open(path, 'rt') if path.suffix == '.gz' else path.open()) as stream:
        header = json.loads(next(stream))
        if header['scenario'] is not None and 'encounter' in header['scenario'] and 'start' not in header['scenario']:
            return set(), 0
        rows = [json.loads(line) for line in stream]
    result = set()
    bosses = set()
    for index, row in enumerate(rows):
        snapshot = row['snapshot']
        objects = snapshot['objects']
        room = objects[snapshot['room']['ref']]
        screen = snapshot['screen']
        action = row.get('action', {})
        player = fields(snapshot, snapshot['player'])
        result.add(('room', room['class']))
        if index == 0 and header['scenario'] is not None and 'start' in header['scenario'] and header['scenario']['start']['room'] == 'EventRoom':
            result.add(('question', room['class']))
        for obj in objects:
            if obj['class'].startswith('events.') and obj['class'] not in ('events.GenericEventDialog', 'events.RoomEventDialog'):
                result.add(('event', obj['class']))
        if snapshot['run']['actNum'] == 3 and room['class'] == 'rooms.MonsterRoomBoss':
            bosses.add((snapshot['run']['floorNum'], snapshot['run']['bossKey']))
        if 'terminal' in row:
            result.add(('terminal', row['terminal']))
            continue
        following = rows[index + 1]['snapshot']
        next_player = fields(following, following['player'])
        next_room = following['objects'][following['room']['ref']]
        for key, before, after in zip(('ruby', 'emerald', 'sapphire'), snapshot['run']['keys'], following['run']['keys']):
            if not before and after:
                result.add(('key', key))
        if following['run']['actNum'] != snapshot['run']['actNum']:
            result.add(('transition', str(snapshot['run']['actNum']) + '->' + str(following['run']['actNum'])))
        if screen['type'] == 'GRID':
            grid = fields(snapshot, screen['state'])
            if grid['GridCardSelectScreen.forPurge'] and player['AbstractCreature.gold'] > next_player['AbstractCreature.gold'] and following['screen']['type'] == 'SHOP':
                result.add(('purchase', 'purge'))
        if action['type'] != 'choose':
            continue
        choice = screen['options'][action['option']]
        if screen['choice'] == 'REST':
            result.add(('rest', choice))
        if screen['choice'] == 'BOSS_REWARD':
            result.add(('boss_relic', 'chosen'))
        if choice == 'bowl':
            result.add(('reward', 'bowl'))
        if screen['choice'] == 'CHEST' and choice == 'open':
            for obj in objects:
                if obj['class'].startswith('rewards.chests.'):
                    result.add(('chest', obj['class']))
        if screen['choice'] == 'SHOP' and player['AbstractCreature.gold'] > next_player['AbstractCreature.gold']:
            for obj in objects:
                f = obj['fields']
                if f.get('AbstractCard.cardID') == choice:
                    result.add(('purchase', 'colorless' if f['AbstractCard.color'] == 'COLORLESS' else 'colored'))
                if f.get('AbstractRelic.relicId') == choice:
                    result.add(('purchase', 'relic'))
                if f.get('AbstractPotion.ID') == choice:
                    result.add(('purchase', 'potion'))
            if any(obj['class'] == 'relics.Courier' for obj in objects):
                result.add(('shop', 'courier_purchase'))
        if room['class'] == 'neow.NeowRoom' and screen['choice'] == 'EVENT':
            event = fields(snapshot, room['fields']['AbstractRoom.event'])
            if event['NeowEvent.screenNum'] == 3:
                reward = fields(snapshot, event['NeowEvent.rewards'][int(choice.split(':')[1])])
                result.add(('neow', reward['NeowReward.type']))
                result.add(('drawback', reward['NeowReward.drawback']))
                if event['NeowEvent.bossCount'] == 0:
                    result.add(('mini', reward['NeowReward.type']))
        if screen['choice'] == 'MAP' and choice.startswith('x='):
            y = 0 if not snapshot['run']['firstRoomChosen'] else snapshot['node'][1] + 1
            x = int(choice[2:])
            if snapshot['map'][y][x]['room'] == 'EventRoom':
                result.add(('question', next_room['class']))
            before = [obj['fields']['AbstractRelic.counter'] for obj in objects if obj['class'] == 'relics.WingBoots']
            after = [obj['fields']['AbstractRelic.counter'] for obj in following['objects'] if obj['class'] == 'relics.WingBoots']
            if before and after and before != after:
                result.add(('map', 'wing_boots'))
    if len(bosses) >= 2:
        result.add(('boss', 'act_three_double'))
    if header['scenario'] is not None and 'encounter' in header['scenario']:
        result |= {('run:' + kind, value) for kind, value in result}
    return result, len(rows) - 1


def required():
    groups = {
        'room': ['neow.NeowRoom'] + ['rooms.' + name for name in ('MonsterRoom', 'MonsterRoomElite', 'MonsterRoomBoss', 'RestRoom', 'ShopRoom', 'TreasureRoom', 'TreasureRoomBoss', 'EventRoom', 'VictoryRoom', 'TrueVictoryRoom')],
        'rest': [name + 'Option' for name in ('Rest', 'Smith', 'Lift', 'Dig', 'Toke', 'Recall')],
        'chest': ['rewards.chests.' + name + 'Chest' for name in ('Small', 'Medium', 'Large', 'Boss')],
        'key': ['ruby', 'emerald', 'sapphire'],
        'transition': ['1->2', '2->3', '3->4'],
        'purchase': ['colored', 'colorless', 'relic', 'potion', 'purge'],
        'question': ['rooms.' + name + 'Room' for name in ('Monster', 'Shop', 'Treasure', 'Event')],
        'shop': ['courier_purchase'], 'reward': ['bowl'], 'boss_relic': ['chosen'],
        'map': ['wing_boots'], 'boss': ['act_three_double'], 'terminal': ['death', 'victory'],
        'mini': ['THREE_ENEMY_KILL', 'TEN_PERCENT_HP_BONUS'],
        'drawback': ['NONE', 'TEN_PERCENT_HP_LOSS', 'NO_GOLD', 'CURSE', 'PERCENT_DAMAGE'],
        'neow': ['RANDOM_COLORLESS_2', 'THREE_CARDS', 'ONE_RANDOM_RARE_CARD', 'REMOVE_CARD', 'UPGRADE_CARD', 'RANDOM_COLORLESS', 'TRANSFORM_CARD', 'THREE_SMALL_POTIONS', 'RANDOM_COMMON_RELIC', 'TEN_PERCENT_HP_BONUS', 'HUNDRED_GOLD', 'THREE_ENEMY_KILL', 'REMOVE_TWO', 'TRANSFORM_TWO_CARDS', 'ONE_RARE_RELIC', 'THREE_RARE_CARDS', 'TWO_FIFTY_GOLD', 'TWENTY_PERCENT_HP_BONUS', 'BOSS_RELIC'],
    }
    result = {(kind, value) for kind, values in groups.items() for value in values}
    run_groups = {'room', 'rest', 'chest', 'key', 'transition', 'purchase', 'boss_relic', 'boss', 'shop', 'reward'}
    return result | {('run:' + kind, value) for kind, value in result if kind in run_groups}


def check(paths):
    observed, decisions = set(), 0
    for path in paths:
        found, count = evidence(path)
        observed |= found
        decisions += count
    missing = required() - observed
    assert not missing, sorted(missing)
    print(f'Run coverage passed: {len(observed)} observations, {decisions} decisions')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='+', type=Path)
    args = parser.parse_args()
    paths = [path for root in args.paths for path in (sorted(root.rglob('*.jsonl')) + sorted(root.rglob('*.jsonl.gz')) if root.is_dir() else [root])]
    check(paths)
