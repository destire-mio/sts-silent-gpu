#!/usr/bin/env python3
"""Build the Silent/A20 port inventory from the CFR Java tree (Python 3 stdlib)."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re

SOURCE = Path('/Users/destire/Documents/Codex/2026-09-10/new-chat-2/work/native-complete-source/source')
PACKAGE = 'com.megacrit.cardcrawl.'
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_$][\w$]*|\d+(?:\.\d+)?[fFdDlL]?|>>>|>>|<<|==|!=|>=|<=|\+=|-=|\*=|/=|&&|\|\||\+\+|--|[^\s]')
VISUAL = re.compile(r'^(render.*|.*[Ss]fx.*|.*[Ss]ound.*|load.*|.*[Aa]nimation.*|.*[Gg]low.*|.*[Hh]over.*|.*[Pp]article.*|.*[Vv]fx.*|updateDescription|initializeDescription|makeCopy|makeStatEquivalentCopy|updateAnimation|updateParticles|updateHb|updateFont|updateReticle|updateEscapeAnimation|updateDeathAnimation)$')
FLAGS = ('exhaust', 'isEthereal', 'isInnate', 'retain', 'selfRetain', 'isMultiDamage', 'purgeOnUse', 'returnToHand', 'shuffleBackIntoDrawPile', 'exhaustOnUseOnce', 'exhaustOnFire')


def expression(tokens):
    return ' '.join(tokens)


def value(tokens):
    s = expression(tokens)
    if len(tokens) == 1 and tokens[0].startswith('"'):
        return json.loads(tokens[0])
    if s in ('true', 'false'):
        return s == 'true'
    if re.fullmatch(r'-?\s*\d+', s):
        return int(s.replace(' ', ''))
    if re.fullmatch(r'-?\s*\d+(?:\.\d+)?[fFdDlL]?', s):
        return float(re.sub(r'[fFdDlL]$', '', s.replace(' ', '')))
    return {'expression': s}


class Java:
    def __init__(self, path, root):
        self.path = path.relative_to(root).as_posix()
        self.text = path.read_text()
        matches = [m for m in TOKEN.finditer(self.text) if not m[0].startswith(('//', '/*'))]
        self.t = [m[0] for m in matches]
        self.lines = [self.text.count('\n', 0, m.start()) + 1 for m in matches]
        self.pairs = {}
        self.depth = []
        stack = []
        brace_depth = 0
        for i, t in enumerate(self.t):
            self.depth.append(brace_depth)
            if t in ('(', '[', '{'):
                stack.append(i)
                brace_depth += t == '{'
            elif t in (')', ']', '}'):
                start = stack.pop()
                assert self.t[start] == {')': '(', ']': '[', '}': '{'}[t], self.path
                self.pairs[start] = i
                self.pairs[i] = start
                brace_depth -= t == '}'
        assert not stack, self.path
        self.name = path.stem
        self.qualified = self.path[:-5].replace('/', '.')
        self.imports = dict((s.rsplit('.', 1)[1], s) for s in re.findall(r'import ([\w.]+);', self.text))
        parent = re.search(r'public class \w+\s+extends (\w+)', self.text)
        self.parent = self.resolve(parent[1]) if parent else None
        self.methods = []
        for i, t in enumerate(self.t):
            if t != '{' or self.depth[i] != 1 or self.t[i - 1] != ')':
                continue
            paren = self.pairs[i - 1]
            name = self.t[paren - 1]
            start = paren - 2
            while self.t[start] not in (';', '}', '{'):
                start -= 1
            header = self.t[start + 1:paren - 1]
            self.methods.append({'name': name, 'start': i + 1, 'end': self.pairs[i], 'line': self.lines[paren - 1], 'override': 'Override' in header, 'parameters': expression(self.t[paren + 1:i - 1])})
        self.constants = {}
        for m in re.finditer(r'(?:static\s+final|final\s+static)\s+(?:String|byte|int|float|boolean)\s+(\w+)\s*=\s*([^;]+);', self.text):
            ts = TOKEN.findall(m[2])
            self.constants[m[1]] = value(ts)
        self.class_start = next(i for i, token in enumerate(self.t) if token in ('class', 'enum', 'interface'))
        self.controls = self.control_spans()

    def method(self, i):
        return next((m for m in self.methods if m['start'] <= i < m['end']), None)

    def named(self, name):
        return [m for m in self.methods if m['name'] == name]

    def resolve(self, name):
        return self.imports.get(name, self.qualified.rsplit('.', 1)[0] + '.' + name)

    def body_end(self, i):
        if self.t[i] == '{':
            return self.pairs[i] + 1
        if self.t[i] == 'if':
            end = self.body_end(self.pairs[i + 1] + 1)
            if end < len(self.t) and self.t[end] == 'else':
                end = self.body_end(end + 1)
            return end
        while self.t[i] != ';':
            if self.t[i] in ('(', '[', '{'):
                i = self.pairs[i]
            i += 1
        return i + 1

    def control_spans(self):
        spans = []
        for i, t in enumerate(self.t):
            if t == 'if' and self.t[i + 1] == '(':
                close = self.pairs[i + 1]
                expr = expression(self.t[i + 2:close])
                end = self.body_end(close + 1)
                spans.append((close + 1, end, {'if': expr, 'branch': True, 'line': self.lines[i]}))
                if end < len(self.t) and self.t[end] == 'else':
                    spans.append((end + 1, self.body_end(end + 1), {'if': expr, 'branch': False, 'line': self.lines[i]}))
            if t in ('case', 'default'):
                colon = i + 1
                while self.t[colon] != ':':
                    colon += 1
                end = colon + 1
                while end < len(self.t) and not (self.depth[end] == self.depth[i] and self.t[end] in ('case', 'default', '}')):
                    end += 1
                spans.append((colon + 1, end, {'case': expression(self.t[i + 1:colon]), 'line': self.lines[i]}))
        return spans

    def conditions(self, i):
        return [c for start, end, c in self.controls if start <= i < end]

    def at(self, i):
        m = self.method(i)
        return {'line': self.lines[i], 'method': m['name'] if m else None, 'conditions': self.conditions(i)}

    def calls(self, lo=0, hi=None):
        hi = len(self.t) if hi is None else hi
        for i in range(lo, hi - 1):
            if not re.fullmatch(r'[A-Za-z_$][\w$]*', self.t[i]) or self.t[i + 1] != '(' or self.t[i] in ('if', 'switch', 'for', 'while', 'catch', 'synchronized'):
                continue
            end = self.pairs[i + 1]
            if end >= hi:
                continue
            args = []
            start = j = i + 2
            while j < end:
                if self.t[j] in ('(', '[', '{'):
                    j = self.pairs[j]
                elif self.t[j] == ',':
                    args.append(self.t[start:j])
                    start = j + 1
                j += 1
            if start < end:
                args.append(self.t[start:end])
            yield i, self.t[i], args

    def assignments(self, lo, hi):
        for i in range(lo + 1, hi - 1):
            if self.t[i] in ('++', '--'):
                start = i + 1 if re.fullmatch(r'[A-Za-z_$][\w$]*', self.t[i + 1]) else i - 1
                end = start + 1
                if start == i + 1:
                    while end + 1 < hi and self.t[end] == '.':
                        end += 2
                else:
                    while start >= lo + 2 and self.t[start - 1] == '.':
                        start -= 2
                yield i, expression(self.t[start:end]).replace(' ', ''), '+=' if self.t[i] == '++' else '-=', ['1']
                continue
            if self.t[i] not in ('=', '+=', '-=', '*=', '/=') or not re.fullmatch(r'[A-Za-z_$][\w$]*', self.t[i - 1]):
                continue
            start = i - 1
            while start >= lo + 2 and self.t[start - 1] == '.':
                start -= 2
            end = j = i + 1
            while j < hi and self.t[j] not in (';', ','):
                if self.t[j] in ('(', '[', '{'):
                    j = self.pairs[j]
                j += 1
            end = j
            rhs = self.t[i + 1:end]
            while '=' in rhs:
                rhs = rhs[rhs.index('=') + 1:]
            yield i, expression(self.t[start:i]).replace(' ', ''), self.t[i], rhs

    def overrides(self):
        return sorted({m['name'] for m in self.methods if m['override']})

    def game_id(self):
        for key in ('ID', 'POWER_ID', 'POTION_ID', 'ORB_ID', 'STANCE_ID'):
            if isinstance(self.constants.get(key), str):
                return self.constants[key]
        for m in self.methods:
            for i, field, op, rhs in self.assignments(m['start'], m['end']):
                if field == 'this.ID' and op == '=' and isinstance(value(rhs), str):
                    return value(rhs)
        return None

    def evaluate(self, tokens):
        if len(tokens) == 1 and tokens[0] in self.constants:
            return self.constants[tokens[0]]
        if len(tokens) == 3 and tokens[0] == self.name and tokens[1] == '.' and tokens[2] in self.constants:
            return self.constants[tokens[2]]
        return value(tokens)

    def enum(self, name):
        for i in range(len(self.t) - 2):
            if self.t[i:i + 2] == ['enum', name]:
                start = i + 2
                assert self.t[start] == '{'
                end = self.t.index(';', start)
                return [(self.t[j], j) for j in range(start + 1, end) if self.t[j] != ',']
        raise AssertionError((self.path, name))


def card_data(j, defaults):
    constructors = j.named(j.name)
    supers = [(i, args) for m in constructors for i, n, args in j.calls(m['start'], m['end']) if n == 'super']
    assert supers, j.path
    i, args = supers[0]
    data = {'cost': j.evaluate(args[3]), 'type': args[5][-1], 'color': args[6][-1], 'rarity': args[7][-1], 'target': args[8][-1], 'base_damage': defaults['baseDamage'], 'base_block': defaults['baseBlock'], 'base_magic_number': defaults['baseMagicNumber'], 'flags': {k: defaults[k] for k in FLAGS}, 'constructor_variants': [], 'upgrade_changes': [], 'keywords': [], 'tags': []}
    names = {'baseDamage': 'base_damage', 'baseBlock': 'base_block', 'baseMagicNumber': 'base_magic_number'}
    for m in constructors:
        variants = []
        for pos, field, op, rhs in j.assignments(m['start'], m['end']):
            field = field.removeprefix('this.')
            if field not in names and field not in FLAGS:
                continue
            v = j.evaluate(rhs)
            variants.append({'field': field, 'operation': op, 'value': v, **j.at(pos)})
            if not j.conditions(pos) and op == '=':
                if field in names:
                    data[names[field]] = v
                else:
                    data['flags'][field] = v
        data['constructor_variants'].append({'parameters': m['parameters'], 'line': m['line'], 'assignments': variants})
    for m in j.named('upgrade'):
        for pos, name, args in j.calls(m['start'], m['end']):
            if name.startswith('upgrade'):
                data['upgrade_changes'].append({'operation': name, 'arguments': [j.evaluate(a) for a in args], **j.at(pos)})
        for pos, field, op, rhs in j.assignments(m['start'], m['end']):
            field = field.removeprefix('this.')
            if field in FLAGS or field in names or field in ('cost', 'costForTurn', 'magicNumber', 'damage', 'block', 'rawDescription', 'target', 'timesUpgraded'):
                data['upgrade_changes'].append({'field': field, 'operation': op, 'value': j.evaluate(rhs), **j.at(pos)})
    data['keywords'] = sorted({{'exhaust': 'exhaust', 'isEthereal': 'ethereal', 'isInnate': 'innate', 'retain': 'retain', 'selfRetain': 'retain'}.get(k, k) for k, v in data['flags'].items() if v is True and k in ('exhaust', 'isEthereal', 'isInnate', 'retain', 'selfRetain')} | set(re.findall(r'GameDictionary\.(\w+)\.', j.text)))
    data['keyword_basis'] = 'Java behavior flags and GameDictionary references; localized description keywords are not in this Java tree'
    data['tags'] = sorted(set(re.findall(r'tags\.add\((?:AbstractCard\.)?CardTags\.(\w+)\)', j.text)))
    return data


def sizing(j, lo=0, hi=None):
    hi = len(j.t) if hi is None else hi
    methods = [m for m in j.methods if lo <= m['start'] < hi and not VISUAL.fullmatch(m['name'])]
    branches = [{'condition': c, 'line': j.lines[start]} for start, end, c in j.controls if lo <= start < hi and j.method(start) and not VISUAL.fullmatch(j.method(start)['name'])]
    writes = []
    for m in methods:
        for pos, field, op, rhs in j.assignments(m['start'], m['end']):
            if re.search(r'damage|block|magic|cost|health|hp|gold|amount|chance|counter|count|price|energy|key|floor|move|turn|poison|strength|ritual|armor|draw|discard|exhaust|potency|debuff|buff|retain|slot|orb|stance', field, re.I) and not any('Strings' in t or t.startswith('"images/') for t in rhs):
                writes.append({'field': field, 'operation': op, 'value': j.evaluate(rhs), **j.at(pos)})
    return {'conditions': branches, 'state_changes': writes, 'methods': [{'name': m['name'], 'parameters': m['parameters'], 'line': m['line'], 'end_line': j.lines[m['end']]} for m in methods], 'branch_count': len(branches), 'ascension_branches': [b for b in branches if 'ascension' in str(b).lower()], 'numeric_constants': {k: v for k, v in j.constants.items() if isinstance(v, (int, float))}}


def cases(j, method_name):
    for m in j.named(method_name):
        for start, end, c in j.controls:
            if 'case' in c and m['start'] <= start < m['end']:
                yield value(TOKEN.findall(c['case'])), start, end


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    args = parser.parse_args()
    root = args.source
    files = {j.qualified: j for path in sorted((root / 'com/megacrit/cardcrawl').rglob('*.java')) for j in [Java(path, root)]}
    get = lambda s: files[PACKAGE + s]

    def descendant(j, ancestor):
        parent = j.parent
        while parent in files:
            if parent == PACKAGE + ancestor:
                return True
            parent = files[parent].parent
        return False
    units = {}
    owners = {}
    spans = {}
    reasons = defaultdict(list)

    def add(j, kind, stage='standard', facts=None, key=None, game_id=None, lo=0, hi=None):
        key = key or kind + ':' + j.qualified.removeprefix(PACKAGE)
        if key in units:
            if stage == 'standard':
                units[key]['stage'] = stage
            return key
        units[key] = {'key': key, 'kind': kind, 'game_id': j.game_id() if game_id is None else game_id, 'java_path': j.path, 'java_class': j.qualified, 'stage': stage, 'facts': facts or {}, 'sizing': sizing(j, lo, hi), 'depends_on': []}
        spans[key] = (lo, len(j.t) if hi is None else hi)
        if lo == 0 and hi is None:
            owners[j.qualified] = key
        return key

    abstract_card = get('cards.AbstractCard')
    defaults = {}
    for i, field, op, rhs in abstract_card.assignments(0, len(abstract_card.t)):
        if field in (*FLAGS, 'baseDamage', 'baseBlock', 'baseMagicNumber') and field not in defaults:
            defaults[field] = value(rhs)
    assert len(defaults) == len(FLAGS) + 3, defaults
    card_library = get('helpers.CardLibrary')
    registered_cards = {card_library.resolve(n) for i, n, a in card_library.calls() if i and card_library.t[i - 1] == 'new' and card_library.method(i)}
    card_candidates = {q: card_data(files[q], defaults) for q in registered_cards if q.startswith(PACKAGE + 'cards.') and 'extends AbstractCard' in files[q].text}
    for q, data in sorted(card_candidates.items()):
        if data['color'] == 'GREEN' or data['color'] == 'COLORLESS' and data['rarity'] != 'SPECIAL' or data['type'] == 'CURSE' and data['rarity'] != 'SPECIAL':
            add(files[q], 'card', facts=data)
        elif data['color'] in ('RED', 'BLUE', 'PURPLE') and data['rarity'] in ('COMMON', 'UNCOMMON', 'RARE'):
            add(files[q], 'card', 'cross_color_last', data)
    for j in files.values():
        if j.qualified.startswith(PACKAGE + 'cards.status.'):
            add(j, 'card', facts=card_data(j, defaults))

    relic_library = get('helpers.RelicLibrary')
    for i, name, a in relic_library.calls():
        if name not in ('add', 'addGreen') or not a or a[0][0] != 'new':
            continue
        j = files[relic_library.resolve(a[0][1])]
        tier = re.search(r'(?:AbstractRelic\.)?RelicTier\.(\w+)', j.text)[1]
        add(j, 'relic', facts={'tier': tier, 'pool': 'green' if name == 'addGreen' else 'shared', 'hooks': j.overrides()})

    potion_helper = get('helpers.PotionHelper')
    potion_ids = set()
    for m in potion_helper.named('getPotions'):
        for i, n, a in potion_helper.calls(m['start'], m['end']):
            if n != 'add' or not a or not isinstance(value(a[0]), str):
                continue
            cond = potion_helper.conditions(i)
            if not cond or any(c.get('case') == 'THE_SILENT' for c in cond):
                potion_ids.add(value(a[0]))
    for j in files.values():
        if j.qualified.startswith(PACKAGE + 'potions.') and j.game_id() in potion_ids:
            rarity = re.search(r'(?:AbstractPotion\.)?PotionRarity\.(\w+)', j.text)[1]
            add(j, 'potion', facts={'rarity': rarity, 'hooks': j.overrides()})
    assert len([u for u in units.values() if u['kind'] == 'potion']) == len(potion_ids)

    acts = ('Exordium', 'TheCity', 'TheBeyond', 'TheEnding')
    encounter_pools = defaultdict(list)
    event_pools = defaultdict(list)
    for act, name in enumerate(acts, 1):
        j = get('dungeons.' + name)
        for i, n, a in j.calls():
            if n == 'MonsterInfo' and i and j.t[i - 1] == 'new':
                encounter_pools[value(a[0])].append({'act': act, 'pool': j.method(i)['name'], 'weight': value(a[1]), 'java_path': j.path, **j.at(i)})
            if n == 'add' and i >= 2 and j.t[i - 2] in ('bossList', 'monsterList', 'eliteMonsterList', 'eventList', 'shrineList') and a and isinstance(value(a[0]), str):
                field = j.t[i - 2]
                pool = encounter_pools if field in ('bossList', 'monsterList', 'eliteMonsterList') else event_pools
                record = {'act': act, 'pool': field, 'java_path': j.path, **j.at(i)}
                if record not in pool[value(a[0])]:
                    pool[value(a[0])].append(record)
    dungeon = get('dungeons.AbstractDungeon')

    def disabled_helper(j, condition):
        invoked = re.fullmatch(r'(?:this \. )?(\w+) \( \)', condition)
        if not invoked:
            return False
        for m in j.named(invoked[1]):
            for start, end, c in j.controls:
                comparison = re.fullmatch(r'(?:AbstractDungeon \. )?ascensionLevel (>=|>|==|<|<=) (\d+)', c.get('if', ''))
                if comparison and m['start'] <= start < m['end']:
                    number = int(comparison[2])
                    active = {'>=': 20 >= number, '>': 20 > number, '==': 20 == number, '<': 20 < number, '<=': 20 <= number}[comparison[1]]
                    if active and any(j.t[k:k + 3] == ['return', 'false', ';'] for k in range(start, end - 2)):
                        return True
        return False
    for i, n, a in dungeon.calls():
        if n == 'add' and i >= 2 and dungeon.t[i - 2] == 'specialOneTimeEventList':
            # AbstractDungeon.initializeSpecialOneTimeEventList excludes NoteForYourself in ascension runs.
            if any(c.get('branch') is True and disabled_helper(dungeon, c.get('if', '')) for c in dungeon.conditions(i)):
                continue
            event_pools[value(a[0])].append({'act': None, 'pool': 'specialOneTimeEventList', 'java_path': dungeon.path, **dungeon.at(i)})
    event_helper = get('helpers.EventHelper')
    event_cases = {game_id: (start, end) for game_id, start, end in cases(event_helper, 'getEvent')}
    for game_id, pool in sorted(event_pools.items()):
        start, end = event_cases[game_id]
        refs = [(i, n) for i, n, a in event_helper.calls(start, end) if event_helper.t[i - 1] == 'new']
        assert len(refs) == 1, game_id
        j = files[event_helper.resolve(refs[0][1])]
        conditions = []
        for method in ('getEvent', 'getShrine'):
            for cid, a, b in cases(dungeon, method):
                if cid == game_id:
                    conditions.extend({'eligibility_excludes_when': c['if'], 'java_path': dungeon.path, 'line': c['line']} for x, y, c in dungeon.controls if a <= x < b and 'if' in c)
        add(j, 'event', facts={'act_pools': pool, 'eligibility': conditions, 'shrine': any(p['pool'] != 'eventList' for p in pool)}, game_id=game_id)

    monster_helper = get('helpers.MonsterHelper')
    encounter_cases = {cid: (a, b) for cid, a, b in cases(monster_helper, 'getEncounter')}
    for key, u in list(units.items()):
        if u['kind'] != 'event':
            continue
        j = files[u['java_class']]
        for i, n, a in j.calls():
            if n == 'getEncounter' and a and isinstance(j.evaluate(a[0]), str):
                encounter_pools[j.evaluate(a[0])].append({'event': u['game_id'], 'act': None, 'pool': 'event', 'java_path': j.path, **j.at(i)})
        for m in j.methods:
            for k in range(m['start'], m['end'] - 1):
                if j.t[k] == 'return' and j.t[k + 1].startswith('"') and value([j.t[k + 1]]) in encounter_cases:
                    encounter_pools[value([j.t[k + 1]])].append({'event': u['game_id'], 'act': None, 'pool': 'event', 'java_path': j.path, **j.at(k)})
    for cid, pools in sorted(encounter_pools.items()):
        a, b = encounter_cases[cid]
        key = add(monster_helper, 'encounter', facts={'pools': pools, 'factory_line': monster_helper.lines[a]}, key='encounter:' + cid, game_id=cid, lo=a, hi=b)

    rule_classes = [
        'actions.GameActionManager', 'actions.AbstractGameAction', 'cards.AbstractCard', 'cards.CardGroup', 'cards.CardQueueItem', 'cards.DamageInfo',
        'characters.AbstractPlayer', 'characters.TheSilent', 'core.AbstractCreature', 'random.Random', 'dungeons.AbstractDungeon',
        'helpers.CardLibrary', 'helpers.RelicLibrary', 'helpers.PotionHelper', 'helpers.EventHelper', 'helpers.MonsterHelper',
        'map.MapGenerator', 'map.MapRoomNode', 'map.MapEdge', 'map.RoomTypeAssigner', 'map.EdgeComparator',
        'shop.ShopScreen', 'shop.StoreRelic', 'shop.StorePotion', 'rewards.RewardItem',
        'screens.CombatRewardScreen', 'screens.CardRewardScreen', 'screens.DungeonMapScreen', 'screens.DungeonTransitionScreen',
        'screens.select.GridCardSelectScreen', 'screens.select.HandCardSelectScreen', 'screens.select.BossRelicSelectScreen',
        'screens.DoorUnlockScreen', 'screens.DoorLock', 'neow.NeowEvent', 'neow.NeowReward',
        'events.AbstractEvent', 'events.AbstractImageEvent', 'monsters.AbstractMonster', 'monsters.MonsterGroup', 'monsters.MonsterInfo', 'powers.AbstractPower', 'relics.AbstractRelic', 'potions.AbstractPotion',
        'ui.panels.EnergyPanel', 'vfx.ObtainKeyEffect', 'vfx.cardManip.ShowCardAndObtainEffect', 'vfx.cardManip.ShowCardAndAddToDrawPileEffect', 'vfx.cardManip.ShowCardAndAddToDiscardEffect',
        'vfx.cardManip.PurgeCardEffect', 'vfx.UpgradeShineEffect']
    for prefix in ('rooms.', 'rewards.chests.', 'ui.campfire.'):
        rule_classes.extend(q.removeprefix(PACKAGE) for q in files if q.startswith(PACKAGE + prefix))
    rule_classes.extend(q.removeprefix(PACKAGE) for q in files if re.fullmatch(re.escape(PACKAGE) + r'vfx\.campfire\.Campfire(Dig|Lift|Recall|Sleep|Smith|Toke)Effect', q))
    rule_classes.extend('dungeons.' + name for name in acts)
    for name in sorted(set(rule_classes)):
        j = get(name)
        add(j, 'rule', facts={'rule': name, 'hooks': j.overrides()})

    player = get('characters.AbstractPlayer')
    foreign_methods = {m['name'] for m in player.methods if re.search(r'orb|stance', m['name'], re.I) and not VISUAL.fullmatch(m['name'])}
    method_owners = {}
    for m in player.methods:
        if m['name'] in foreign_methods:
            key = 'rule:characters.AbstractPlayer:' + m['name']
            add(player, 'rule', 'cross_color_last', {'rule': m['name'], 'parameters': m['parameters']}, key=key, lo=m['start'], hi=m['end'])
            method_owners[m['name']] = key

    neow = get('neow.NeowReward')
    for enum in ('NeowRewardType', 'NeowRewardDrawback'):
        for option, i in neow.enum(enum):
            if option == 'NONE':
                continue
            sites = []
            for pos, name, a in neow.calls():
                if name in ('NeowRewardDef', 'NeowRewardDrawbackDef') and a and a[0][-1] == option:
                    sites.append(neow.at(pos))
            case_spans = [(a, b) for cid, a, b in cases(neow, 'activate') if cid == {'expression': option}]
            add(neow, 'rule', facts={'enum': enum, 'option': option, 'availability': sites, 'activation_lines': [neow.lines[a] for a, b in case_spans]}, key='rule:neow:' + enum + ':' + option, game_id=option, lo=case_spans[0][0] if case_spans else i, hi=case_spans[0][1] if case_spans else i + 1)

    candidate_kinds = {}
    for q, j in files.items():
        if q.startswith(PACKAGE + 'powers.') and j.name != 'AbstractPower':
            candidate_kinds[q] = 'power'
        elif q.startswith(PACKAGE + 'actions.') and descendant(j, 'actions.AbstractGameAction') and not q.startswith(PACKAGE + 'actions.animations.') and j.name not in ('RelicAboveCreatureAction', 'HideHealthBarAction', 'SFXAction', 'ShakeScreenAction', 'TextAboveCreatureAction', 'WaitAction'):
            candidate_kinds[q] = 'action'
        elif q.startswith(PACKAGE + 'monsters.') and 'extends AbstractMonster' in j.text:
            candidate_kinds[q] = 'monster'
        elif q.startswith(PACKAGE + ('orbs.')) or q.startswith(PACKAGE + 'stances.'):
            candidate_kinds[q] = 'rule'
        elif q in card_candidates or q.startswith(PACKAGE + 'cards.status.'):
            candidate_kinds[q] = 'card'
        elif q.startswith(PACKAGE + 'relics.') and (q in owners or j.name == 'Circlet'):
            candidate_kinds[q] = 'relic'
    ids = defaultdict(set)
    for q, j in files.items():
        if q in candidate_kinds and j.game_id():
            ids[j.game_id()].add(q)

    def references(j, lo, hi, include_rule_content=False):
        refs = defaultdict(list)
        obtainable_relics = {u['game_id'] for u in units.values() if u['kind'] == 'relic'}
        for i in range(max(lo, j.class_start), hi):
            m = j.method(i)
            if m and VISUAL.fullmatch(m['name']):
                continue
            conditions = j.conditions(i)
            if any(c.get('branch') is True and (c.get('if') in ('Settings . isEndless', 'Settings . isDailyRun') or 'ModHelper . isModEnabled' in c.get('if', '') or disabled_helper(j, c.get('if', ''))) for c in conditions):
                continue
            impossible_relic = False
            for c in conditions:
                test = re.fullmatch(r'(?:this|player|AbstractDungeon \. player) \. hasRelic \( ("[^"]+") \)', c.get('if', ''))
                if test and c.get('branch') is True and json.loads(test[1]) not in obtainable_relics:
                    impossible_relic = True
            if impossible_relic:
                continue
            if j.name == 'MonsterHelper' and include_rule_content and m and m['name'] in ('getEncounter', 'getEncounterName'):
                continue
            if m and m['name'] in ('addRedCards', 'addBlueCards', 'addPurpleCards', 'addColorlessCards', 'addCurseCards', 'addGreenCards', 'getPotions', 'initialize') and j.name in ('CardLibrary', 'RelicLibrary', 'PotionHelper'):
                continue
            if j.t[i].startswith('"'):
                literal = value([j.t[i]])
                for pos, name, a in j.calls(max(lo, i - 12), min(hi, i + 16)):
                    if name in ('getRelic', 'hasRelic', 'loseRelic', 'getCard', 'hasPower', 'getPower', 'removePower', 'RemoveSpecificPowerAction', 'ReducePowerAction', 'getPotion') and any([j.t[i]] == arg for arg in a):
                        for q in ids[literal]:
                            refs[q].append(j.at(i))
                continue
            q = j.resolve(j.t[i])
            if q not in files:
                continue
            if i > lo and j.t[i - 1] == 'import':
                continue
            if q in candidate_kinds or q in owners:
                refs[q].append(j.at(i))
        if j.name == 'MonsterHelper' and not include_rule_content:
            work = [(lo, hi)]
            visited = set()
            while work:
                a, b = work.pop()
                for i, n, args in j.calls(a, b):
                    if n in visited or n in ('getEncounter', 'getEncounterName'):
                        continue
                    visited.add(n)
                    for m in j.named(n):
                        for k in range(m['start'], m['end']):
                            if k and j.t[k - 1] == 'new':
                                q = j.resolve(j.t[k])
                                if q in candidate_kinds:
                                    refs[q].append(j.at(k))
                        work.append((m['start'], m['end']))
        return refs

    def factory_facts(j, lo, hi):
        pending_spans = [(lo, hi)]
        seen_methods = set()
        constructors = []
        helpers = []
        random_calls = []
        selection_pools = []
        while pending_spans:
            a, b = pending_spans.pop(0)
            for i, n, args in j.calls(a, b):
                q = j.resolve(n)
                if i and j.t[i - 1] == 'new' and q in candidate_kinds and candidate_kinds[q] == 'monster':
                    statement = i - 1
                    while statement > a and j.t[statement - 1] not in (';', '{', '}'):
                        statement -= 1
                    prefix = j.t[statement:i]
                    assignment = prefix[:prefix.index('=')] if '=' in prefix else []
                    selection = None
                    if '?' in prefix:
                        question = prefix.index('?')
                        predicate_start = prefix.index('=') + 1 if '=' in prefix else 0
                        selection = {'predicate': expression(prefix[predicate_start:question]), 'branch': ':' not in prefix[question + 1:]}
                    constructors.append({'java_class': q, 'arguments': [j.evaluate(arg) for arg in args], 'assigned_to': expression(assignment) or None, 'selection': selection, **j.at(i)})
                if n.startswith('random'):
                    random_calls.append({'operation': n, 'arguments': [j.evaluate(arg) for arg in args], **j.at(i)})
                if n == 'add' and args and isinstance(j.evaluate(args[0]), str):
                    selection_pools.append({'entry': j.evaluate(args[0]), **j.at(i)})
                if n not in seen_methods and n not in ('getEncounter', 'getEncounterName'):
                    seen_methods.add(n)
                    for m in j.named(n):
                        pending_spans.append((m['start'], m['end']))
                        helpers.append({'name': n, 'line': m['line'], 'call_arguments': [j.evaluate(arg) for arg in args]})
        return {'monster_constructors': constructors, 'helper_methods': helpers, 'random_calls': random_calls, 'selection_pool_entries': selection_pools}

    for key, u in units.items():
        if u['kind'] == 'encounter':
            u['facts'].update(factory_facts(files[u['java_class']], *spans[key]))

    def reached(j, kind, stage):
        if kind == 'card':
            facts = card_candidates.get(j.qualified)
            if facts is None:
                facts = card_data(j, defaults)
        elif kind in ('relic', 'power'):
            facts = {'hooks': j.overrides()}
            if kind == 'relic':
                facts['tier'] = re.search(r'(?:AbstractRelic\.)?RelicTier\.(\w+)', j.text)[1]
                facts['pool'] = 'referenced'
        elif kind == 'monster':
            moves = []
            for cid, a, b in cases(j, 'takeTurn'):
                moves.append({'move_id': cid, 'line': j.lines[a], 'action_classes': sorted({n for i, n, args in j.calls(a, b) if i and j.t[i - 1] == 'new' and n.endswith('Action')}), 'conditions': [c for x, y, c in j.controls if a <= x < b and 'if' in c]})
            intents = [{'arguments': [j.evaluate(a) for a in args], **j.at(i)} for i, n, args in j.calls() if n == 'setMove']
            hp = [{'arguments': [j.evaluate(a) for a in args], **j.at(i)} for i, n, args in j.calls() if n == 'setHp']
            damage = [{'arguments': [j.evaluate(a) for a in args], **j.at(i)} for i, n, args in j.calls() if n == 'DamageInfo' and i and j.t[i - 1] == 'new']
            facts = {'moves': moves, 'intents': intents, 'hp': hp, 'damage': damage, 'move_constants': {k: v for k, v in j.constants.items() if isinstance(v, int)}, 'hooks': j.overrides()}
        else:
            facts = {'hooks': j.overrides()}
        return add(j, kind, stage, facts)

    excluded_methods = {'rule:characters.AbstractPlayer': foreign_methods, 'rule:neow.NeowReward': {'activate'}, 'rule:helpers.MonsterHelper': {'getEncounter', 'getEncounterName'}}
    for key, names in excluded_methods.items():
        u = units[key]
        u['facts']['excluded_methods'] = sorted(names)
        u['sizing']['methods'] = [m for m in u['sizing']['methods'] if m['name'] not in names]
        u['sizing']['state_changes'] = [w for w in u['sizing']['state_changes'] if w['method'] not in names]

    pending = list(units)
    processed = {}
    while pending:
        key = pending.pop(0)
        u = units[key]
        if processed.get(key) == u['stage']:
            continue
        processed[key] = u['stage']
        j = files[u['java_class']]
        lo, hi = spans[key]
        refs = references(j, lo, hi, u['kind'] == 'rule')
        if key in excluded_methods:
            refs = {q: [site for site in sites if site['method'] not in excluded_methods[key]] for q, sites in refs.items()}
            refs = {q: sites for q, sites in refs.items() if sites}
        for q, sites in sorted(refs.items()):
            if q == j.qualified:
                continue
            if q not in owners and q not in candidate_kinds:
                continue
            if u['kind'] == 'rule' and q in candidate_kinds and candidate_kinds[q] in ('card', 'relic') and q not in owners:
                if all(any('endless' in c.get('if', '').lower() or 'ModHelper' in c.get('if', '') for c in site['conditions']) for site in sites):
                    continue
                if q.startswith(PACKAGE + 'relics.') and q not in owners and not files[q].name == 'Circlet':
                    continue
            if q in owners:
                dependency = owners[q]
                old = units[dependency]['stage']
                source_cross = u['kind'] == 'rule' and all(site['method'] and re.search(r'orb|stance', site['method'], re.I) for site in sites)
                foreign = q in card_candidates and card_candidates[q]['color'] in ('RED', 'BLUE', 'PURPLE') or q.startswith(PACKAGE + 'orbs.') and files[q].name not in ('AbstractOrb', 'EmptyOrbSlot') or q.startswith(PACKAGE + 'stances.') and files[q].name not in ('AbstractStance', 'NeutralStance')
                creates_content = candidate_kinds.get(q) not in ('power', 'card', 'monster', 'relic') or any(j.t[pos - 1] == 'new' and j.resolve(j.t[pos]) == q and j.lines[pos] in {site['line'] for site in sites} for pos in range(lo, hi))
                if u['stage'] == 'standard' and old != 'standard' and not foreign and not source_cross and creates_content:
                    units[dependency]['stage'] = 'standard'
                    pending.append(dependency)
            else:
                if candidate_kinds[q] != 'rule' and not any(j.t[pos - 1] == 'new' and j.resolve(j.t[pos]) == q for pos in range(lo, hi)) and u['kind'] != 'encounter':
                    continue
                foreign_mechanic = q.startswith(PACKAGE + 'orbs.') and files[q].name not in ('AbstractOrb', 'EmptyOrbSlot') or q.startswith(PACKAGE + 'stances.') and files[q].name not in ('AbstractStance', 'NeutralStance')
                source_cross = u['kind'] == 'rule' and all(site['method'] and re.search(r'orb|stance', site['method'], re.I) for site in sites)
                dependency = reached(files[q], candidate_kinds[q], 'cross_color_last' if foreign_mechanic or source_cross else u['stage'])
                pending.append(dependency)
            if dependency != key:
                u['depends_on'].append(dependency)
                reasons[(key, dependency)].extend(sites)
        for i, n, args in j.calls(lo, hi):
            if n in method_owners and method_owners[n] != key and (j.method(i) is None or j.method(i)['name'] not in excluded_methods.get(key, set())):
                u['depends_on'].append(method_owners[n])
                reasons[(key, method_owners[n])].append(j.at(i))
            if n == 'getEncounter' and args and isinstance(j.evaluate(args[0]), str):
                target = 'encounter:' + j.evaluate(args[0])
                if target in units and target != key:
                    u['depends_on'].append(target)
                    reasons[(key, target)].append(j.at(i))

    for key, u in list(units.items()):
        j = files[u['java_class']]
        for q, sites in references(j, *spans[key], u['kind'] == 'rule').items():
            if q in owners and owners[q] != key:
                sites = [site for site in sites if site['method'] not in excluded_methods.get(key, set())]
                if not sites:
                    continue
                dependency = owners[q]
                u['depends_on'].append(dependency)
                reasons[(key, dependency)].extend(sites)

    ascension_sites = defaultdict(list)
    for j in files.values():
        if j.qualified not in owners:
            continue
        for i, token in enumerate(j.t):
            if token not in ('ascensionLevel', 'ascension'):
                continue
            m = j.method(i)
            if not m or m['name'].lower().startswith(('save', 'load')) or VISUAL.fullmatch(m['name']):
                continue
            for op, literal in [(j.t[i + 1], j.t[i + 2])]:
                if op not in ('>=', '>', '<', '<=', '==', '!=') or not literal.isdigit():
                    continue
                threshold = int(literal)
                level = threshold + 1 if op in ('>', '<=') else threshold
                if 1 <= level <= 20:
                    end = i + 3
                    while end < len(j.t) and j.t[end] not in (';', '{'):
                        end += 1
                    branches = []
                    for start, stop, c in j.controls:
                        if 'if' in c and re.search(r'ascension(?:Level)? ' + re.escape(op) + r' ' + literal + r'\b', c['if']) and c['line'] == j.lines[i]:
                            calls = [{'operation': name, 'arguments': [j.evaluate(arg) for arg in args], **j.at(pos)} for pos, name, args in j.calls(start, stop) if name in ('setHp', 'setMove', 'heal', 'gainGold', 'loseGold') or name.endswith(('Power', 'Action')) and not name.startswith(('SFX', 'VFX', 'Animate', 'Talk', 'Shout', 'Shake'))]
                            branches.append({'condition': c, 'calls': calls, 'state_changes': [{'field': field, 'operation': operation, 'value': j.evaluate(rhs), **j.at(pos)} for pos, field, operation, rhs in j.assignments(start, stop)]})
                    ascension_sites[level].append({'branches': branches, 'java_path': j.path, 'java_class': j.qualified, 'line': j.lines[i], 'method': m['name'], 'comparison': expression(j.t[i:i + 3]), 'expression': expression(j.t[i:end]), 'unit': owners[j.qualified]})
    for level in range(1, 21):
        key = 'ascension:' + str(level)
        units[key] = {'key': key, 'kind': 'ascension', 'game_id': None, 'java_path': dungeon.path, 'java_class': dungeon.qualified, 'stage': 'standard', 'facts': {'level': level, 'cumulative': True, 'source_sites': ascension_sites[level]}, 'depends_on': sorted({s['unit'] for s in ascension_sites[level]})}
        assert ascension_sites[level], level

    for key, u in units.items():
        u['depends_on'] = sorted(set(u['depends_on']))
        u['dependency_sites'] = {d: sorted({(s['line'], s['method']) for s in reasons[(key, d)]}, key=lambda s: (s[0], s[1] or '')) for d in u['depends_on'] if reasons[(key, d)]}
    for key, u in units.items():
        seen = {key}
        work = list(u['depends_on'])
        effects = set()
        while work:
            d = work.pop()
            if d in seen:
                continue
            seen.add(d)
            if units[d]['kind'] in ('card', 'power', 'monster'):
                effects.add(d)
            if units[d]['kind'] == 'action':
                work.extend(units[d]['depends_on'])
        u['content_dependencies'] = sorted(effects)

    groups = defaultdict(list)
    for key, u in units.items():
        groups[(u['stage'], u['kind'])].append(key)
    kind_order = {'rule': 0, 'action': 1, 'power': 2, 'card': 3, 'relic': 4, 'potion': 5, 'monster': 6, 'encounter': 7, 'event': 8, 'ascension': 9}
    batches = []
    for (stage, kind), keys in sorted(groups.items(), key=lambda item: (item[0][0] != 'standard', kind_order[item[0][1]])):
        assert len(keys) >= 10, (stage, kind, len(keys))
        keys.sort(key=lambda k: (len([d for d in units[k]['depends_on'] if units[d]['kind'] == kind]), k))
        count = (len(keys) + 24) // 25
        size, remainder = divmod(len(keys), count)
        offset = 0
        for index in range(count):
            n = size + (index < remainder)
            batch_keys = keys[offset:offset + n]
            offset += n
            batches.append({'id': stage + ':' + kind + ':' + str(index + 1).zfill(2), 'kind': kind, 'stage': stage, 'units': batch_keys, 'depends_on': []})
    batch_of = {k: b['id'] for b in batches for k in b['units']}
    for b in batches:
        b['depends_on'] = sorted({batch_of[d] for k in b['units'] for d in units[k]['depends_on'] if batch_of[d] != b['id']})
    batch_index = {b['id']: index for index, b in enumerate(batches)}
    for b in batches:
        b['requires_batches'] = [d for d in b['depends_on'] if batch_index[d] < batch_index[b['id']] and d.split(':')[1] != b['kind']]
        b['integration_dependencies'] = [d for d in b['depends_on'] if d not in b['requires_batches']]
    totals = Counter(u['kind'] for u in units.values())
    digest = hashlib.sha256()
    for q, j in sorted(files.items()):
        digest.update(j.path.encode())
        digest.update(j.text.encode())
    output = Path(__file__).resolve().parent
    document = {'schema_version': 1, 'target': {'character': 'THE_SILENT', 'ascension': 20, 'acts': [1, 2, 3, 4], 'mode': 'standard', 'unlocks': 'all'}, 'source': {'root': str(root), 'java_tree_sha256': digest.hexdigest()}, 'semantics': {'game_id': 'Java string ID or enum constant; null for rules/actions/ascension without a game string ID', 'depends_on': 'direct source references; dependency_sites give line and method in this row java_path (encounters can include MonsterHelper helper methods)', 'content_dependencies': 'cards/powers/monsters reached through intermediate actions', 'stage': 'cross_color_last content is available only via foreign card generation/rewards and associated mechanics', 'facts': 'data and source expressions, not executable Java; conditional initialization is in constructor_variants', 'keywords': 'source behavior keywords; localization-only keywords require the localization resources', 'completion': 'not stored; computed from oracle replay results', 'external_algorithms': [{'java_class': q, 'referenced_by': sorted({j.path for j in files.values() if j.qualified in owners and q in j.imports.values()}), 'source_available': (root / (q.replace('.', '/') + '.java')).exists()} for q in sorted({q for j in files.values() if j.qualified in owners for q in j.imports.values() if q in ('com.badlogic.gdx.math.RandomXS128', 'java.util.Random', 'java.util.Collections', 'com.badlogic.gdx.math.MathUtils')})]}, 'totals': dict(sorted(totals.items())), 'units': sorted(units.values(), key=lambda u: u['key'])}
    rows = document.pop('units')
    header = json.dumps(document, indent=2, ensure_ascii=False).removesuffix('\n}')
    encoded_rows = ',\n'.join('    ' + json.dumps(row, separators=(',', ':'), ensure_ascii=False) for row in rows)
    (output / 'units.json').write_text(header + ',\n  \"units\": [\n' + encoded_rows + '\n  ]\n}\n')
    (output / 'batches.json').write_text(json.dumps({'schema_version': 1, 'ordering': 'array order defines implementation layers: foundations, actions, powers, cards, relics, potions, monsters, encounters, events, ascension; foreign mechanics last. requires_batches is acyclic; integration_dependencies are links resolved at integration, including same-kind hooks and foundation dispatch to content' , 'batches': batches}, indent=2) + '\n')
    for kind, total in sorted(totals.items()):
        print(f'{kind}: {total}')
    print(f'total: {sum(totals.values())}; batches: {len(batches)}')


if __name__ == '__main__':
    main()
