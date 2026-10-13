import gzip
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SCHEMA = json.loads((ROOT.parent / 'inventory/snapshot-fields.json').read_text())


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
            assert set(obj['fields']) == fields(obj['class']), (path, obj['class'])
        if 'action' in row:
            assert row['action'] in row['legal'] and len(row['legal']) > 0
    first = rows[1]['snapshot']
    player = first['objects'][first['player']['ref']]['fields']
    if header['scenario'] is not None:
        scenario = header['scenario']
        assert first['run']['floorNum'] == 1
        room = first['objects'][first['room']['ref']]
        assert room['class'] == 'rooms.MonsterRoom'
        assert room['fields']['AbstractRoom.phase'] == 'COMBAT'
        assert player['AbstractCreature.currentHealth'] == scenario['hp']
        assert player['AbstractCreature.maxHealth'] == scenario['max_hp']
        deck = first['objects'][player['AbstractPlayer.masterDeck']['ref']]['fields']['CardGroup.group']
        cards = [first['objects'][ref['ref']]['fields'] for ref in deck]
        assert [(c['AbstractCard.cardID'], c['AbstractCard.timesUpgraded']) for c in cards] == [
            (c['id'], c['upgrades']) for c in scenario['deck']]
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
    assert len(paths) == 7
    decisions = sum(check(path) for path in paths)
    print(f'{len(paths)} fixtures, {decisions} decisions: schema, legal membership, terminal and scenario setup passed')
