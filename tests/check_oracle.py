import gzip
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SCHEMA = json.loads((ROOT.parent / 'inventory/snapshot-fields.json').read_text())
UNITS = json.loads((ROOT.parent / 'inventory/units.json').read_text())['units']
CARDS = {u['game_id']: u for u in UNITS if u['kind'] == 'card'}
RELICS = {u['game_id']: u for u in UNITS if u['kind'] == 'relic'}


def fields(class_name):
    cls = SCHEMA[class_name]
    result = {class_name.rsplit('.', 1)[-1] + '.' + name for name in cls['fields']}
    if cls['extends'] is not None:
        result |= fields(cls['extends'])
    return result


def check(path):
    with gzip.open(path, 'rt') as stream:
        rows = [json.loads(line) for line in stream]
    header, terminal = rows[0], rows[-1]
    assert header['schema'] == 2
    assert header['character'] == 'THE_SILENT' and header['ascension'] == 20
    assert terminal['terminal'] in ('death', 'combat_end', 'victory')
    assert len(rows) >= 3
    for row in rows[1:]:
        snapshot = row['snapshot']
        objects = snapshot['objects']
        for obj in objects:
            assert obj['class'] != 'actions.utility.WaitAction', path
            assert set(obj['fields']) == fields(obj['class']), (path, obj['class'])
        if 'action' in row:
            assert row['action'] in row['legal'] and len(row['legal']) > 0
    first = rows[1]['snapshot']
    player = first['objects'][first['player']['ref']]['fields']
    if header['scenario'] is not None:
        scenario = header['scenario']
        start = scenario.get('start')
        assert first['run']['floorNum'] == (start['floor'] if start else 1)
        assert first['run']['actNum'] == (start['act'] if start else 1)
        room = first['objects'][first['room']['ref']]
        if start is None:
            assert room['class'] == 'rooms.MonsterRoom'
            assert room['fields']['AbstractRoom.phase'] == 'COMBAT'
        elif start['room'] != 'EventRoom':
            prefix = 'neow.' if start['room'] == 'NeowRoom' else 'rooms.'
            assert room['class'] == prefix + start['room']
        # AbstractDungeon.nextRoomTransition runs relic room-entry and battle-start hooks.
        if not scenario['relics']:
            assert player['AbstractCreature.currentHealth'] == scenario['hp'], path
            assert player['AbstractCreature.maxHealth'] == scenario['max_hp'], path
        deck = first['objects'][player['AbstractPlayer.masterDeck']['ref']]['fields']['CardGroup.group']
        cards = [first['objects'][ref['ref']]['fields'] for ref in deck]
        # Recorder.scenario equips relics before entering combat; onEquip may replace the deck.
        if not any('onEquip' in RELICS[r]['facts']['hooks'] for r in scenario['relics']):
            assert [(c['AbstractCard.cardID'], c['AbstractCard.timesUpgraded']) for c in cards] == [
                (c['id'], c['upgrades'] if CARDS[c['id']]['facts']['upgrade_changes'] else 0)
                for c in scenario['deck']], path
    else:
        assert first['run']['floorNum'] == 0
        assert first['objects'][first['room']['ref']]['class'] == 'neow.NeowRoom'
    if path.name == 'Strike_G.jsonl.gz':
        snapshots = [row['snapshot'] for row in rows[1:]]
        counters = [snapshot['rng']['unseeded'][0] for snapshot in snapshots]
        for row, before, after in zip(rows[1:-1], counters, counters[1:]):
            assert after - before == (1 if row['action']['type'] == 'end_turn' else 0)
    return len(rows) - 2


if __name__ == '__main__':
    paths = sorted((ROOT / 'traces').glob('*.jsonl.gz'))
    assert ROOT / 'traces/full-run-1.jsonl.gz' in paths
    decisions = sum(check(path) for path in paths)
    print(f'{len(paths)} fixtures, {decisions} decisions: schema, legal membership, terminal and scenario setup passed')
