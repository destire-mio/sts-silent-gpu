"""Measure game-rule coverage using only strict replays of original recordings."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from collections import Counter, defaultdict

ROOT = Path(__file__).resolve().parents[1]
LLVM = Path('/opt/homebrew/opt/llvm/bin')
sys.path.insert(0, str(ROOT / 'tests'))
from replay import trace_paths

JAVA_METHODS = {
    'choose_campfire': ('vfx.campfire.CampfireSleepEffect', 'update'),
    'finish_campfire_selection': ('vfx.campfire.CampfireSmithEffect', 'update'),
    'make_card': ('cards.AbstractCard', '<init>'),
    'update_card_cost': ('cards.AbstractCard', 'updateCost'),
    'card_damage_value': ('cards.AbstractCard', 'calculateCardDamage'),
    'card_has_enough_energy_base': ('cards.AbstractCard', 'hasEnoughEnergy'),
    'play_card_hooks': ('characters.AbstractPlayer', 'useCard'),
    'use_card': ('characters.AbstractPlayer', 'useCard'),
    'apply_power': ('actions.common.ApplyPowerAction', 'update'),
    'apply_prepared_power': ('actions.common.ApplyPowerAction', 'update'),
    'calculate_damage': ('cards.DamageInfo', 'applyPowers'),
    'calculate_intent_damage': ('monsters.AbstractMonster', 'calculateDamage'),
    'random_living_monster': ('dungeons.AbstractDungeon', 'getRandomMonster'),
    'reduce_power': ('actions.common.ReducePowerAction', 'update'),
    'first_strong_excluded': ('dungeons.AbstractDungeon', 'populateFirstStrongEnemy'),
    'roll_encounter': ('dungeons.AbstractDungeon', 'populateMonsterList'),
    'advance_escape': ('monsters.AbstractMonster', 'updateEscapeAnimation'),
    'begin_player_turn': ('actions.GameActionManager', 'getNextAction'),
    'close_grid_screen': ('dungeons.AbstractDungeon', 'closeCurrentScreen'),
    'complete_master_selection': ('screens.select.GridCardSelectScreen', 'update'),
    'complete_player_turn_effect': ('actions.GameActionManager', 'getNextAction'),
    'draw_cards': ('actions.common.DrawCardAction', 'update'),
    'end_turn_early': ('actions.GameActionManager', 'callEndTurnEarlySequence'),
    'enter_combat': ('dungeons.AbstractDungeon', 'nextRoomTransition'),
    'enter_map_node': ('dungeons.AbstractDungeon', 'nextRoomTransition'),
    'enter_room': ('dungeons.AbstractDungeon', 'nextRoomTransition'),
    'foreign_attack_card': ('cards.purple.ForeignInfluence', 'use'),
    'heal_creature': ('characters.AbstractPlayer', 'heal'),
    'monster_apply_powers': ('monsters.AbstractMonster', 'applyPowers'),
    'monster_damage_base': ('monsters.AbstractMonster', 'damage'),
    'monster_die_base': ('monsters.AbstractMonster', 'die'),
    'on_draw_or_discard': ('characters.AbstractPlayer', 'onCardDrawOrDiscard'),
    'open_hand_selection': ('screens.select.HandCardSelectScreen', 'open'),
    'player_damage': ('characters.AbstractPlayer', 'damage'),
    'prepare_player_battle': ('characters.AbstractPlayer', 'preBattlePrep'),
    'recharge_energy': ('core.EnergyManager', 'recharge'),
    'run_until_decision': ('actions.GameActionManager', 'update'),
    'settle_deaths_after_wait': ('monsters.AbstractMonster', 'updateDeathAnimation'),
    'sort_selection_cards': ('screens.select.GridCardSelectScreen', 'open'),
    'enter_event': ('dungeons.AbstractDungeon', 'generateEvent'),
    'event_add_option': ('events.RoomEventDialog', 'addDialogOption'),
    'event_purge_cards': ('cards.CardGroup', 'getPurgeableCards'),
    'event_remove_option': ('events.RoomEventDialog', 'removeDialogOption'),
    'roll_event_room': ('helpers.EventHelper', 'roll'),
    'add_room_gold': ('rooms.AbstractRoom', 'addGoldToRewards'),
    'create_card_reward': ('dungeons.AbstractDungeon', 'getRewardCards'),
    'equip_relic': ('relics.AbstractRelic', 'obtain'),
    'gain_gold': ('characters.AbstractPlayer', 'gainGold'),
    'lose_gold': ('characters.AbstractPlayer', 'loseGold'),
    'master_card_by_uuid': ('cards.CardGroup', 'getSpecificCard'),
    'obtain_card': ('vfx.cardManip.ShowCardAndObtainEffect', 'update'),
    'random_colorless_card': ('dungeons.AbstractDungeon', 'returnColorlessCard'),
    'random_relic_key': ('dungeons.AbstractDungeon', 'returnRandomRelicKey'),
    'remove_master_card': ('cards.CardGroup', 'removeCard'),
    'remove_room_relic_reward': ('rooms.AbstractRoom', 'removeOneRelicFromRewards'),
    'reward_card_choices': ('dungeons.AbstractDungeon', 'getRewardCards'),
    'shuffled_colorless_card': ('dungeons.AbstractDungeon', 'returnColorlessCard'),
    'transform_card': ('dungeons.AbstractDungeon', 'transformCard'),
    'unequip_relic': ('characters.AbstractPlayer', 'loseRelic'),
    'monster_on_damage': ('monsters.AbstractMonster', 'damage'),
    'lightning_damage': ('actions.defect.LightningOrbEvokeAction', 'update'),
    'orb_base_evoke': ('orbs.AbstractOrb', '<init>'),
    'orb_base_passive': ('orbs.AbstractOrb', '<init>'),
    'add_noncamp_relic_reward': ('rooms.AbstractRoom', 'addNoncampRelicToRewards'),
    'combat_rewards': ('rooms.AbstractRoom', 'dropReward'),
    'create_colorless_reward': ('dungeons.AbstractDungeon', 'getColorlessRewardCards'),
    'random_relic_tier': ('dungeons.AbstractDungeon', 'returnRandomRelicTier'),
    'enter_shop': ('shop.ShopScreen', 'init'),
    'finish_shop_purge': ('shop.ShopScreen', 'updatePurge'),
    'purchase_shop': ('shop.ShopScreen', 'purchaseCard'),
    'shop_discount': ('shop.ShopScreen', 'applyDiscount'),
    'shop_potion_price': ('shop.StorePotion', '<init>'),
    'shop_restock_price': ('shop.ShopScreen', 'getNewPrice'),
    'change_stance': ('actions.watcher.ChangeStanceAction', 'update'),
    'choose_boss_relic': ('screens.select.BossRelicSelectScreen', 'update'),
    'enter_treasure': ('rewards.chests.AbstractChest', '<init>'),
    'open_chest': ('rewards.chests.AbstractChest', 'open'),
    'step': ('rooms.ShopRoom', 'updatePurge'),
}

UNIT_JAVA_METHODS = {
    ('PhilosopherStone', 'add_strength'): ('relics.PhilosopherStone', 'atBattleStart'),
    ('Omamori', 'onCurseObtain'): ('vfx.cardManip.ShowCardAndObtainEffect', '<init>'),
    ('EntropicBrew', 'constructPotion'): ('potions.EntropicBrew', '<init>'),
    ('JawWorm', 'bellow_block'): ('monsters.exordium.JawWorm', '<init>'),
    ('JawWorm', 'bellow_strength'): ('monsters.exordium.JawWorm', '<init>'),
    ('TheLibrary', 'draw'): ('events.city.TheLibrary', 'getBook'),
    ('Looter', 'gold_amount'): ('monsters.exordium.Looter', '<init>'),
    ('Mugger', 'gold_amount'): ('monsters.city.Mugger', '<init>'),
    ('RemoveSpecificPowerAction', 'from_instance'): ('actions.common.RemoveSpecificPowerAction', '<init>'),
    ('Falling', 'pick'): ('events.beyond.Falling', 'setCards'),
    ('SplitPower', 'initialPower'): ('powers.SplitPower', '<init>'),
    ('DeadAdventurer', 'encounter'): ('events.exordium.DeadAdventurer', 'getMonster'),
    ('AttackFromDeckToHandAction', 'move_card'): ('actions.unique.AttackFromDeckToHandAction', 'update'),
    ('ChooseOneColorless', 'generate_card_choices'): ('actions.utility.ChooseOneColorless', 'generateCardChoices'),
    ('Designer', 'upgrade_random'): ('events.shrines.Designer', 'upgradeTwoRandomCards'),
    ('NoBlockPower', 'initFromSource'): ('powers.NoBlockPower', '<init>'),
    ('BetterDrawPileToHandAction', 'move'): ('actions.common.BetterDrawPileToHandAction', 'update'),
    ('ExhumeAction', 'move'): ('actions.unique.ExhumeAction', 'update'),
    ('MeditateAction', 'move'): ('actions.watcher.MeditateAction', 'update'),
    ('RecycleAction', 'recycle'): ('actions.defect.RecycleAction', 'update'),
    ('Blizzard', 'frost_count'): ('cards.blue.Blizzard', 'applyPowers'),
    ('ForceField', 'constructCard'): ('cards.blue.ForceField', 'configureCostsOnNewCard'),
    ('SearingBlow', 'makeStatEquivalentCopy'): ('cards.AbstractCard', 'makeStatEquivalentCopy'),
}


def run(command, log, env=None):
    with (log.parent / 'commands.jsonl').open('a') as stream:
        stream.write(json.dumps(dict(command=list(map(str, command)), log=str(log))) + '\n')
    print(f'{command[0]}: {log.name}', flush=True)
    with log.open('w') as stream:
        result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                stdout=stream, stderr=subprocess.STDOUT)
    lines = log.read_text().splitlines()
    print(lines[-1] if lines else f'exit {result.returncode}', flush=True)
    return result.returncode


def rule_file(filename, build):
    path = Path(filename)
    if path.is_relative_to(ROOT / 'core'):
        source = path.with_suffix('.inc')
        return source if source.exists() else path
    generated = build / 'spire_generated'
    if path.is_relative_to(generated):
        relative = path.relative_to(generated)
        if relative.parts[0] == 'core':
            source = (ROOT / relative).with_suffix('.inc')
            return source if source.exists() else path
        if relative.name in ('content.hpp', 'bases.hpp', 'ids.hpp'):
            return path
        if relative.parts[0] == 'sources' and not relative.name.startswith('obs_'):
            return path
    return None


def ownership():
    units = json.loads((ROOT / 'inventory/units.json').read_text())['units']
    result = {}
    for catalog in (ROOT / 'core').glob('*/catalog.def'):
        for enum, game_id, filename, typename in re.findall(
                r'SPIRE_UNIT\((\w+), "([^"]+)", "([^"]+)", (\w+)\)', catalog.read_text()):
            kind = catalog.parent.name[:-1]
            matches = [u for u in units if u['kind'] == kind and
                       (u['game_id'] == game_id or u['java_class'].endswith('.' + game_id))]
            if matches:
                unit = matches[0]
            else:
                text = (ROOT / filename).read_text()
                java = game_id if kind == 'action' else 'potions.PotionSlot' if kind == 'potion' else re.search(r'"((?:events|neow)\.[^"]+)"', text)[1]
                unit = dict(key=kind + ':' + java, kind=kind, java_class='com.megacrit.cardcrawl.' + java,
                            java_path='com/megacrit/cardcrawl/' + java.replace('.', '/') + '.java')
            unit = dict(unit, header=filename)
            result[filename] = unit
            if kind == 'card':
                result['@card-id:' + enum] = unit
            result['@' + catalog.parent.name + ':' + typename] = unit
    for path in (ROOT / 'core').glob('*/*.inc'):
        text = path.read_text()
        for key in re.findall(r'SPIRE_RULE\("([^"]+)"\)', text):
            original, = [u for u in units if u['key'] == key]
            unit = dict(original, header=str(path.relative_to(ROOT)))
            result[str(path.relative_to(ROOT))] = unit
            typename = re.search(r'\bstruct (\w+)\s*[:{]', text)
            if typename:
                result['@' + path.parent.name + ':' + typename[1]] = unit
    return result


def java_basis(function, source, owners, line):
    relative = str(source.relative_to(ROOT)) if source.is_relative_to(ROOT) else str(source)
    unit = owners.get(relative)
    symbol = re.search(r'::units::(\w+)::(\w+)::', function)
    if symbol:
        typename = symbol[2].removesuffix('Data') if symbol[2].endswith('Data') else symbol[2]
        unit = owners['@' + symbol[1] + ':' + typename]
    method = function.split('(')[0].rsplit('::', 1)[-1]
    method = re.sub(r'<.*>', '', method)
    if method in ('card_rarity', 'card_sort_order', 'card_colorless', 'card_name_order'):
        statement = source.read_text().splitlines()[line - 1]
        case = re.search(r'case CardId::(\w+):', statement)
        if case:
            unit = owners['@card-id:' + case[1]]
            caller = {'card_rarity': 'cards.CardGroup.getRandomCard',
                      'card_sort_order': 'cards.AbstractCard.compareTo',
                      'card_colorless': 'dungeons.AbstractDungeon.transformCard',
                      'card_name_order': 'cards.CardGroup.sortAlphabetically'}[method]
            return dict(owning_unit=unit['key'], java_class=unit['java_class'], java_method='<init>',
                        java_path=unit['java_path'], generated_property=method,
                        java_rule_caller='com.megacrit.cardcrawl.' + caller,
                        mapping='Generated property consumed by a Java rule; constructor defines card color, rarity, ID or localized name')
    if source.name == 'random.hpp':
        xs = 'RandomXS128::' in function or method in ('rng_murmur_hash', 'signed_long', 'set_state')
        java = 'com.badlogic.gdx.math.RandomXS128' if xs else 'com.megacrit.cardcrawl.random.Random'
        method = {'reset': 'setSeed' if xs else '<init>', 'next_bits64': 'nextLong',
                  'set_state': 'setState', 'rng_murmur_hash': 'murmurHash3',
                  'signed_long': 'nextLong', 'java_long_cast': 'random'}.get(method, method)
        return dict(owning_unit='rule:random.Random', java_class=java, java_method=method,
                    java_path=java.replace('.', '/') + '.java', mapping='Java RNG wrapper or libGDX generator')
    method = {'execute': 'update', 'prepare': '<init>', 'construct': '<init>',
              'make': '<init>', 'data': '<init>', 'value': '<init>', 'enter': '<init>',
              'choose': 'buttonEffect', 'build': '<init>', 'constructPower': '<init>',
              'damageGive': 'atDamageGive', 'damageReceive': 'atDamageReceive'}.get(method, method)
    if unit:
        if unit['kind'] == 'rule' and unit['key'].count(':') == 2:
            method = unit['key'].rsplit(':', 1)[-1]
        java = unit['java_class'].removeprefix('com.megacrit.cardcrawl.')
        java, method = UNIT_JAVA_METHODS.get((java.rsplit('.', 1)[-1], method), (java, method))
        return dict(owning_unit=unit['key'], java_class='com.megacrit.cardcrawl.' + java,
                    java_method=method, java_path='com/megacrit/cardcrawl/' + java.replace('.', '/') + '.java',
                    mapping='unit hook or original helper body')
    if method == 'apply_decision':
        before = '\n'.join(source.read_text().splitlines()[:line])
        screens = re.findall(r'if \([^\n]*g.screen == Screen::(Room|BossReward|Event|Shop|Map|CombatReward|CardReward|Grid|Discard|Combat)\b', before)
        java, java_method = {
            'Room': ('ui.buttons.ProceedButton', 'update'),
            'BossReward': ('screens.select.BossRelicSelectScreen', 'update'),
            'Event': ('events.RoomEventDialog', 'update'),
            'Shop': ('shop.ShopScreen', 'update'),
            'Map': ('dungeons.AbstractDungeon', 'nextRoomTransition'),
            'CombatReward': ('rewards.RewardItem', 'claimReward'),
            'CardReward': ('screens.CardRewardScreen', 'update'),
            'Grid': ('screens.select.GridCardSelectScreen', 'update'),
            'Discard': ('screens.select.HandCardSelectScreen', 'update'),
            'Combat': ('characters.AbstractPlayer', 'useCard'),
        }[screens[-1] if screens else 'Combat']
        return dict(owning_unit='rule:' + java, java_class='com.megacrit.cardcrawl.' + java,
                    java_method=java_method, java_path='com/megacrit/cardcrawl/' + java.replace('.', '/') + '.java',
                    mapping='original screen callback consumed by engine action adapter')
    if method in JAVA_METHODS:
        java, method = JAVA_METHODS[method]
        if source.name == 'campfire.inc':
            before = source.read_text().splitlines()[:line]
            options = re.findall(r'case CampfireOption::(\w+)', '\n'.join(before))
            if options and options[-1] != 'Count':
                java = 'vfx.campfire.Campfire' + {'Rest': 'Sleep', 'Smith': 'Smith', 'Toke': 'Toke',
                                                'Lift': 'Lift', 'Dig': 'Dig', 'Recall': 'Recall'}[options[-1]] + 'Effect'
        return dict(owning_unit='rule:' + java, java_class='com.megacrit.cardcrawl.' + java,
                    java_method=method, java_path='com/megacrit/cardcrawl/' + java.replace('.', '/') + '.java',
                    mapping='shared Java rule service')
    return dict(owning_unit='engine:' + source.stem, java_class=None, java_method=None,
                java_path=None, mapping='requires classification of C++ engine control or Java call-site triage')


def source_location(filename, line, build, function, owners):
    path = Path(filename)
    source = rule_file(filename, build)
    logical = line
    if path.is_relative_to(build / 'spire_generated/sources'):
        for physical, text in enumerate(path.read_text().splitlines()[:line], 1):
            directive = re.fullmatch(r'#line (\d+) "([^"]+)"', text)
            if directive and not Path(directive[2]).is_relative_to(build / 'spire_generated'):
                source = rule_file(directive[2], build) or Path(directive[2])
                logical = int(directive[1]) + line - physical - 1
    symbol = re.search(r'::units::(\w+)::(\w+)::(\w+)\(', function)
    if symbol:
        typename = symbol[2].removesuffix('Data')
        owner = owners['@' + symbol[1] + ':' + typename]
        original = ROOT / owner['header']
        if symbol[3] in ('data', 'value'):
            source = original
            logical = next(i for i, text in enumerate(original.read_text().splitlines(), 1)
                           if re.search(r'\bdata\(', text))
    return source, logical


def analyze(export, build):
    owners = ownership()
    files = {f['filename']: f for f in export['data'][0]['files']
             if rule_file(f['filename'], build)}
    functions = export['data'][0]['functions']
    names = subprocess.run([LLVM / 'llvm-cxxfilt'], input='\n'.join(f['name'] for f in functions),
                           text=True, capture_output=True, check=True).stdout.splitlines()
    groups = defaultdict(list)
    for function, name in zip(functions, names):
        filename = function['filenames'][0]
        if filename not in files:
            continue
        outer = next(r for r in function['regions'] if r[5] == 0 and r[7] == 0)
        groups[(filename, *outer[:4])].append((function, name))
    selected = [max(group, key=lambda item: sum(r[4] > 0 for r in item[0]['regions'] if r[7] == 0))
                for group in groups.values()]
    identities = {(tuple(f['filenames']), tuple(tuple(r) for r in f['regions'])) for f, _ in selected}
    region_counts, branch_counts = {}, {}
    function_spans = defaultdict(list)
    for function, name in selected:
        filename = function['filenames'][0]
        if filename not in files:
            continue
        outer = next(r for r in function['regions'] if r[5] == 0 and r[7] == 0)
        function_spans[filename].append((*outer[:4], name))
        for region in function['regions']:
            line, column, end_line, end_column, count, file_id, expanded, kind = region
            if file_id != 0 or kind != 0:
                continue
            key = (filename, line, column, end_line, end_column)
            entry = region_counts.setdefault(key, dict(count=0, function=name))
            entry['count'] += count
        for branch in function['branches']:
            line, column, end_line, end_column, true_count, false_count, file_id, expanded, kind = branch
            if file_id == 0:
                key = (filename, line, column, end_line, end_column)
                entry = branch_counts.setdefault(key, dict(true_count=0, false_count=0, function=name))
                entry['true_count'] += true_count
                entry['false_count'] += false_count
    for filename, file in files.items():
        for expansion in file['expansions']:
            identity = (tuple(expansion['filenames']), tuple(tuple(r) for r in expansion['target_regions']))
            if identity not in identities:
                continue
            line, column, end_line, end_column, count, source_id, target_id, kind = expansion['source_region']
            span = min((span for span in function_spans[filename]
                        if (span[0], span[1]) <= (line, column) < (span[2], span[3])),
                       key=lambda span: (span[2] - span[0], span[3] - span[1]))
            for records, counts, branch in ((expansion['target_regions'], region_counts, False),
                                            (expansion['branches'], branch_counts, True)):
                duplicates = Counter()
                for record in records:
                    file_id, region_kind = (record[6], record[8]) if branch else (record[5], record[7])
                    if file_id != target_id or region_kind != (4 if branch else 0):
                        continue
                    geometry = tuple(record[:4])
                    duplicates[geometry] += 1
                    macro = (geometry, duplicates[geometry])
                    key = (filename, line, column, end_line, end_column, macro)
                    initial = dict(true_count=0, false_count=0) if branch else dict(count=0)
                    entry = counts.setdefault(key, dict(function=span[4], macro_definition=dict(
                        file=expansion['filenames'][target_id], line=record[0], column=record[1],
                        end_line=record[2], end_column=record[3], ordinal=duplicates[geometry]), **initial))
                    if branch:
                        entry['true_count'] += record[4]
                        entry['false_count'] += record[5]
                    else:
                        entry['count'] += record[4]
    regions, branches = [], []
    for counts, target in ((region_counts, regions), (branch_counts, branches)):
        for key, entry in counts.items():
            filename, line, column, end_line, end_column = key[:5]
            missing = entry['count'] == 0 if target is regions else entry['true_count'] == 0 or entry['false_count'] == 0
            if not missing:
                continue
            source, logical = source_location(filename, line, build, entry['function'], owners)
            target.append(dict(file=str(source), line=logical, column=column,
                               end_line=logical + end_line - line, end_column=end_column,
                               location=f'{source}:{logical}', generated_file=filename, generated_line=line,
                               **entry, **java_basis(entry['function'], source, owners, logical)))
    assert len(regions) == sum(f['summary']['regions']['notcovered'] for f in files.values())
    totals = {}
    areas = {}
    for filename, entry in files.items():
        source = rule_file(filename, build)
        relative = source.relative_to(ROOT / 'core') if source.is_relative_to(ROOT / 'core') else None
        area = relative.parts[0] if relative and len(relative.parts) > 1 else 'engine' if relative else 'generated'
        if Path(filename).parent.name == 'sources':
            group = Path(filename).stem.split('_')[0]
            area = 'engine' if group == 'core' else 'generated' if group == 'dispatch' else group
        values = areas.setdefault(area, {})
        for metric in ('regions', 'branches', 'lines'):
            for target in (values, totals):
                item = target.setdefault(metric, dict(count=0, covered=0))
                item['count'] += entry['summary'][metric]['count']
                item['covered'] += entry['summary'][metric]['covered']
    for values in [totals, *areas.values()]:
        for item in values.values():
            item['percent'] = 100 * item['covered'] / item['count'] if item['count'] else 100
    instantiations = []
    for group in groups.values():
        for function, name in group:
            filename = function['filenames'][0]
            outer = next(r for r in function['regions'] if r[5] == 0 and r[7] == 0)
            source, line = source_location(filename, outer[0], build, name, owners)
            code = [r for r in function['regions'] if r[7] == 0]
            identity = (tuple(function['filenames']), tuple(tuple(r) for r in function['regions']))
            instantiations.append(dict(function=name, count=function['count'], file=str(source), line=line,
                                       regions=len(code), covered=sum(r[4] > 0 for r in code),
                                       selected_for_file_summary=identity in identities,
                                       **java_basis(name, source, owners, line)))
    return files, dict(overall=totals, areas=areas), regions, branches, instantiations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='Fresh build directory outside the checkout')
    parser.add_argument('--output', type=Path, required=True, help='Fresh evidence directory outside the checkout')
    parser.add_argument('--jobs', type=int, choices=(1, 2), default=2, help='Concurrent build processes')
    parser.add_argument('traces', nargs='+', type=Path)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    assert not build.is_relative_to(ROOT) and not output.is_relative_to(ROOT)
    build.mkdir(parents=True)
    output.mkdir(parents=True)
    profiles = output / 'profiles'
    profiles.mkdir()
    python = os.environ['SPIRE_PYTHON']
    flags = '-fprofile-instr-generate -fcoverage-mapping'
    configure = ['cmake', '-S', ROOT, '-B', build, '-DCMAKE_BUILD_TYPE=Release',
                 '-DCMAKE_CXX_COMPILER=' + str(LLVM / 'clang++'), '-DCMAKE_CUDA_COMPILER=NOTFOUND',
                 '-DPython_EXECUTABLE=' + python, '-DBUILD_TESTING=ON',
                 '-DCMAKE_CXX_FLAGS=' + flags, '-DCMAKE_CXX_FLAGS_RELEASE=-O0 -DNDEBUG',
                 '-DCMAKE_SHARED_LINKER_FLAGS=' + flags, '-DCMAKE_EXE_LINKER_FLAGS=' + flags]
    assert run(configure, output / 'configure.log') == 0
    assert run(['cmake', '--build', build, '--target', 'spire', '--parallel', str(args.jobs)], output / 'build.log') == 0
    corpus = sorted(set(path.resolve() for path in trace_paths(args.traces)))
    assert corpus
    manifest = []
    for path in corpus:
        opener = gzip.open if path.suffix == '.gz' else open
        with opener(path, 'rt') as stream:
            header = json.loads(next(stream))
        assert header['game_sha256'] == 'dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a'
        assert header['schema'] == 2 and header['character'] == 'THE_SILENT' and header['ascension'] == 20
        manifest.append(dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                             seed=header['seed']))
    (output / 'corpus.json').write_text(json.dumps(manifest, indent=2) + '\n')
    env = dict(os.environ, PYTHONPATH=str(build) + ':' + str(ROOT),
               LLVM_PROFILE_FILE=str(profiles / 'replay-%p.profraw'),
               OMP_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1', VECLIB_MAXIMUM_THREADS='1')
    statuses = []
    for index in range(0, len(corpus), 128):
        statuses.append(run([python, ROOT / 'tests/replay.py', *corpus[index:index + 128]],
                            output / f'replay-{index // 128:03}.log', env))
    raw = sorted(profiles.glob('*.profraw'))
    assert raw
    assert run([LLVM / 'llvm-profdata', 'merge', '-sparse', *raw, '-o', output / 'merged.profdata'],
               output / 'merge.log') == 0
    module, = build.glob('spire.*.so')
    command = [LLVM / 'llvm-cov', 'export', module, '-instr-profile=' + str(output / 'merged.profdata')]
    with (output / 'coverage.json').open('w') as stream:
        subprocess.run(command, stdout=stream, check=True)
    export = json.loads((output / 'coverage.json').read_text())
    files, summary, regions, branches, instantiations = analyze(export, build)
    summary['all_replays_passed'] = all(status == 0 for status in statuses)
    summary['trace_count'] = len(corpus)
    summary['commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    for name, value in [('summary', summary), ('uncovered-regions', regions), ('uncovered-branches', branches),
                        ('function-coverage', instantiations)]:
        (output / (name + '.json')).write_text(json.dumps(value, indent=2) + '\n')
    assert run([LLVM / 'llvm-cov', 'report', module, '-instr-profile=' + str(output / 'merged.profdata'),
                '-show-region-summary', '-show-branch-summary', *sorted(files)], output / 'report.txt') == 0
    print(json.dumps(summary), flush=True)
    return int(not summary['all_replays_passed'])


if __name__ == '__main__':
    raise SystemExit(main())
