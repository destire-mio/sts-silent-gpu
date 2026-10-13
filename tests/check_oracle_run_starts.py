import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'oracle'))
from record import run_jobs


def fields(snapshot, reference):
    return snapshot['objects'][reference['ref']]['fields']


def deck(snapshot):
    player = fields(snapshot, snapshot['player'])
    group = fields(snapshot, player['AbstractPlayer.masterDeck'])
    return [fields(snapshot, card) for card in group['CardGroup.group']]


def jobs(output):
    start = dict(act=4, floor=52, node=[3, 0], room='RestRoom', gold=2000,
                 keys=[True, True, True], emerald=False)
    mirror = dict(deck=[dict(id='Grand Finale', upgrades=1)] * 5,
                  relics=['Ring of the Snake'], potions=[None, None], hp=1000,
                  max_hp=1000, encounter='The Heart', start=start)
    heart = dict(deck=[dict(id='Bouncing Flask', upgrades=1)] * 3 +
                 [dict(id='Catalyst', upgrades=1)] * 2,
                 relics=['Ring of the Snake', 'Snake Skull', 'Incense Burner',
                         'Lizard Tail', 'TungstenRod', 'Torii', 'Ice Cream'],
                 potions=[None, None], hp=10000, max_hp=10000,
                 encounter='The Heart', start=dict(start, gold=0))
    return [dict(seed=seed, scenario=scenario, output=str(output / name))
            for seed, scenario, name in [(204, mirror, 'mirror.jsonl'), (205, heart, 'heart.jsonl')]]


def check(output):
    recordings = jobs(output)
    run_jobs(recordings)
    rows = [json.loads(line) for line in Path(recordings[0]['output']).read_text().splitlines()][1:]
    copied = False
    for before, after in zip(rows, rows[1:]):
        snapshot = before['snapshot']
        pending = [obj for obj in snapshot['objects'] if obj['class'] == 'relics.DollysMirror'
                   and not obj['fields']['DollysMirror.cardSelected']]
        if snapshot['screen']['type'] != 'GRID' or not pending:
            continue
        assert before['action']['type'] == 'choose'
        selected = snapshot['screen']['options'][before['action']['option']]
        following = after['snapshot']
        assert following['screen']['type'] == 'SHOP'
        assert following['run']['floorNum'] == snapshot['run']['floorNum']
        assert fields(following, following['player'])['AbstractCreature.gold'] == fields(snapshot, snapshot['player'])['AbstractCreature.gold']
        assert following['globals']['shop'] == snapshot['globals']['shop']
        assert len(deck(following)) == len(deck(snapshot)) + 1
        assert deck(following)[-1]['AbstractCard.cardID'] == selected
        assert any(obj['class'] == 'relics.DollysMirror' and obj['fields']['DollysMirror.cardSelected']
                   for obj in following['objects'])
        copied = True
    assert copied
    assert rows[-1]['terminal'] in ('death', 'victory')
    rows = [json.loads(line) for line in Path(recordings[1]['output']).read_text().splitlines()][1:]
    rooms = {row['snapshot']['objects'][row['snapshot']['room']['ref']]['class'] for row in rows}
    assert {'rooms.RestRoom', 'rooms.ShopRoom', 'rooms.MonsterRoomElite', 'rooms.MonsterRoomBoss'} <= rooms
    assert rows[0]['snapshot']['run']['actNum'] == 4
    assert rows[0]['snapshot']['run']['floorNum'] == 52
    assert rows[-1]['terminal'] == 'victory'
    assert rows[-1]['snapshot']['run']['is_victory']
    assert rows[-1]['snapshot']['run']['lastCombatMetricKey'] == 'The Heart'
    print('Run starts: shop mirror copies without purge; Act 4 reaches victory through Shield and Spear and the Heart')


if __name__ == '__main__':
    output = Path(tempfile.mkdtemp(prefix='oracle-run-starts-'))
    print('Run-start output:', output, flush=True)
    check(output)
