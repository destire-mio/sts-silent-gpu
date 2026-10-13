import argparse
import json
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from record import run_jobs

ROOT = Path(__file__).resolve().parent.parent


def suite(output):
    jobs = [dict(seed=i, scenario=None, output=str(output / 'full' / f'{i}.jsonl')) for i in range(1, 21)]
    starter = json.loads((ROOT / 'tests/scenarios/starter.json').read_text())
    cards = ['Strike_G', 'Defend_G', 'Neutralize', 'Survivor', 'AscendersBane',
             'Prepared', 'Dagger Throw', 'Backflip', 'Acrobatics', 'Blade Dance',
             'Deadly Poison', 'Bane', 'Cloak And Dagger', 'Concentrate', 'Well Laid Plans',
             'Burst', 'Night Terror', 'Tools of the Trade', 'Doppelganger', 'Venomology']
    for i in range(200):
        card = cards[i % len(cards)]
        scenario = dict(starter)
        scenario['deck'] = [dict(id=card, upgrades=0 if card == 'AscendersBane' else (i // len(cards)) % 2)] + starter['deck']
        scenario['encounter'] = ['Cultist', 'Jaw Worm', '2 Louse', 'Small Slimes'][i % 4]
        scenario['relics'] = [['Ring of the Snake'], ['Tingsha'], ['Tough Bandages'], ['Ice Cream'], ['OrangePellets']][i % 5]
        scenario['potions'] = [['Fire Potion', None], ['Dexterity Potion', None],
                               ['GamblersBrew', None], ['LiquidMemories', None], [None, None]][i % 5]
        scenario['hp'] = 50 + i % 21
        scenario['max_hp'] = 70
        if i >= 193:
            scenario['relics'] = [['Bottled Lightning'], ['Bottled Flame'], ["Pandora's Box"],
                                  ['Astrolabe'], ['Empty Cage'], ['Tiny House'], ['Potion Belt']][i - 193]
        jobs.append(dict(seed=1000 + i, scenario=scenario, output=str(output / 'scenario' / f'{i}.jsonl')))
    return jobs


def check_trace(path):
    with path.open() as stream:
        header = json.loads(next(stream))
        assert header['schema'] == 1 and header['ascension'] == 20
        decisions = 0
        for text in stream:
            row = json.loads(text)
            if 'terminal' in row:
                assert next(stream, None) is None
                assert row['terminal'] in ('death', 'victory', 'combat_end')
                return decisions
            assert row['action'] in row['legal']
            snapshot = row['snapshot']
            assert len(snapshot['rng']) == 15
            assert snapshot['objects'][snapshot['player']['ref']]['class'] == 'characters.TheSilent'
            room = snapshot['objects'][snapshot['room']['ref']]['fields']
            if room['AbstractRoom.phase'] == 'COMBAT' and not snapshot['screen']['up']:
                manager = snapshot['objects'][snapshot['manager']['ref']]['fields']
                assert manager['GameActionManager.phase'] == 'WAITING_ON_USER'
                for queue in ('actions', 'preTurnActions', 'cardQueue'):
                    assert not manager['GameActionManager.' + queue]
            decisions += 1
    raise AssertionError(f'Missing terminal snapshot: {path}')


def difference(left, right, path='$'):
    if type(left) is not type(right):
        return path, left, right
    if isinstance(left, dict):
        if left.keys() != right.keys():
            return path, list(left), list(right)
        for key in left:
            found = difference(left[key], right[key], path + '.' + key)
            if found:
                return found
    elif isinstance(left, list):
        if len(left) != len(right):
            return path + '.length', len(left), len(right)
        for i, (a, b) in enumerate(zip(left, right)):
            found = difference(a, b, f'{path}[{i}]')
            if found:
                return found
    elif left != right:
        return path, left, right


def compare(left, right):
    a, b = left.read_bytes(), right.read_bytes()
    if a == b:
        return
    for line, (x, y) in enumerate(zip(a.splitlines(), b.splitlines()), 1):
        if x != y:
            raise AssertionError((str(left), line, difference(json.loads(x), json.loads(y))))
    raise AssertionError((str(left), 'line count', len(a.splitlines()), len(b.splitlines())))


def verify(output):
    a, b = output / 'a', output / 'b'
    first = suite(a)
    second = list(reversed(suite(b)))
    with ThreadPoolExecutor(max_workers=2) as pool:
        result_a, result_b = list(pool.map(run_jobs, (first, second)))
    count = 0
    for job in first:
        left = Path(job['output'])
        right = b / left.relative_to(a)
        compare(left, right)
        count += check_trace(left)
    print(json.dumps(dict(byte_identical_full_runs=20, byte_identical_scenarios=200,
                          decisions_per_batch=count, batches=[result_a, result_b])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compare two JVMs and reversed run order byte for byte')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    verify(args.output.resolve())
