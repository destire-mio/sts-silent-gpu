"""Compile unit catalogs into device-safe IDs, base hooks and switch dispatch."""
import argparse
import json
import re
from pathlib import Path
from split_sources import close, emit, masked, write

KINDS = {
    "cards": ("CardId", "CardHooks", "card", "get_card(g, card).id"),
    "powers": ("PowerId", "PowerHooks", "power", "p.id"),
    "relics": ("RelicId", "RelicHooks", "relic", "g.run.relics[relic].id"),
    "potions": ("PotionId", "PotionHooks", "potion", "g.run.potions[slot]"),
    "monsters": ("MonsterId", "MonsterHooks", "monster", "g.monsters[owner].id"),
    "encounters": ("EncounterId", None, "encounter", "id"),
    "events": ("EventId", "EventHooks", "event", "g.event.id"),
    "actions": ("EffectKind", "ActionHooks", "action", "e.kind"),
}
HOOKS = {kind: [] for kind in KINDS}


def hook(kind, name, params, result="void", body="", selector=None):
    HOOKS[kind].append((name, params, result, body, selector))


def power(name, suffix="", result="void", body="", const=False):
    prefix = "const Game& g, const Power& p, int owner" if const else "Game& g, Power& p, int owner"
    hook("powers", name, prefix + (", " + suffix if suffix else ""), result, body)


power("stackPower", "int amount", body="if (p.amount != -1) p.amount += amount;")
power("reducePower", "int amount", body="p.amount = max(0, p.amount - amount);")
power("initialPower")
power("constructPower", "int source, int extra, int card", body="power_initialize_state(p); power_initialPower(g, p, owner);")
power("initFromSource", "bool monster_source", body="p.just_applied = monster_source;")
hook("powers", "isDebuff", "const Power& p, int owner", "bool", "return power_data(p.id).debuff;")
for name in ("atDamageGive", "atDamageReceive", "atDamageFinalGive", "atDamageFinalReceive"):
    power(name, "float value, DamageType type, int card = -1", "float", "return value;", True)
power("modifyBlock", "float value, int card = -1", "float", "return value;", True)
power("modifyBlockLast", "float value", "float", "return value;", True)
for name in ("atStartOfTurn", "duringTurn", "atStartOfTurnPostDraw", "atEndOfRound", "onScry",
             "onSpecificTrigger", "onDeath", "atEnergyGain", "onRemove", "onEnergyRecharge",
             "onDrawOrDiscard", "onInitialApplication", "onVictory"):
    power(name)
for name in ("atEndOfTurn", "atEndOfTurnPreEndTurnCards"):
    power(name, "bool is_player")
for name in ("onCardDraw", "onExhaust", "onAfterCardPlayed", "triggerMarks"):
    power(name, "int card")
for name in ("onUseCard", "onAfterUseCard"):
    power(name, "int card, Effect& use")
power("onPlayCard", "int card, int target")
for name in ("onAttacked", "onAttackedToChangeDamage", "onAttackToChangeDamage"):
    power(name, "const DamageInfo& info, int amount", "int", "return amount;")
for name in ("onAttack", "onInflictDamage"):
    power(name, "const DamageInfo& info, int amount, int target")
power("wasHPLost", "const DamageInfo& info, int amount")
for name in ("onHeal", "onLoseHp"):
    power(name, "int amount", "int", "return amount;")
power("onDamageAllEnemies", "int* amounts")
power("onGainedBlock", "float amount")
power("onPlayerGainedBlock", "float amount", "int", "return floor_int(amount);")
power("onPlayerGainedBlock", "int amount", "int", "return amount;")
power("onGainCharge", "int amount")
power("onApplyPower", "const Power& applied, int target, int source")
power("canPlayCard", "int card", "bool", "return true;", True)
for name in ("onEvokeOrb", "onChannel"):
    power(name, "int orb")
power("onChangeStance", "int old_stance, int new_stance")


def relic(name, suffix="", result="void", body="", const=False):
    prefix = "const Game& g, int relic" if const else "Game& g, int relic"
    hook("relics", name, prefix + (", " + suffix if suffix else ""), result, body)

hook("relics", "initializeState", "Relic& relic", selector="relic.id")
relic("onCurseObtain", result="bool", body="return false;")
relic("referencesMasterCard", "uint32_t uuid", "bool", "return false;", True)


for name in ("onGainGold", "onLoseGold", "onSpendGold", "onEquip", "onUnequip", "atPreBattle",
             "atBattleStart", "atBattleStartPreDraw", "atTurnStart", "atTurnStartPostDraw",
             "onPlayerEndTurn", "onBloodied", "onNotBloodied", "onManualDiscard", "onVictory",
             "onMeditate", "onEnergyRecharge", "onRest", "onRitual", "onEnterRestRoom", "onRefreshHand",
             "onShuffle", "onSmith", "onDrawOrDiscard", "onMasterDeckChange", "onUsePotion", "disableUntilTurnEnds", "beforeEnergyPrep"):
    relic(name)
relic("update")
relic("setCounter", "int counter", body="g.run.relics[relic].counter = counter;")
for name in ("onExhaust", "onCardDraw"):
    relic(name, "int card")
for name in ("onPreviewObtainCard", "onObtainCard"):
    relic(name, "Card& card")
relic("onPlayCard", "int card, int target")
relic("onUseCard", "int card, Effect& use")
for name in ("onSpawnMonster", "onMonsterDeath", "onBlockBroken"):
    relic(name, "int target")
for name in ("onPlayerGainBlock", "onPlayerHeal", "onLoseHpLast"):
    relic(name, "int amount", "int", "return amount;")
relic("onPlayerGainedBlock", "float amount", "int", "return floor_int(amount);")
for name in ("onLoseHp", "wasHPLost"):
    relic(name, "int amount")
for name in ("onAttacked", "onAttackedToChangeDamage", "onAttackToChangeDamage"):
    relic(name, "const DamageInfo& info, int amount", "int", "return amount;")
relic("onAttack", "const DamageInfo& info, int amount, int target")
relic("atDamageModify", "float value, int card", "float", "return value;", True)
for name in ("changeNumberOfCardsInReward", "changeRareCardRewardChance", "changeUncommonCardRewardChance"):
    relic(name, "int amount", "int", "return amount;")
for name in ("canPlay", "canUseCampfireOption"):
    relic(name, "int item", "bool", "return true;", True)
for name in ("canSpawn", "checkTrigger"):
    relic(name, result="bool", body="return false;" if name == "checkTrigger" else "return true;", const=True)
relic("getPrice", result="int", body="return relic_data(g.run.relics[relic].id).price;", const=True)
relic("onTrigger")
relic("onTrigger", "int target")
relic("onEvokeOrb", "int orb")
relic("onChangeStance", "int old_stance, int new_stance")
for name in ("onChestOpen", "onChestOpenAfter"):
    relic(name, "bool boss_chest")
for name in ("onEnterRoom", "justEnteredRoom"):
    relic(name, "int room")
relic("addCampfireOption", "CampfireOptions& options")


def card(name, suffix="", result="void", body="", const=False):
    prefix = "const Game& g, int card" if const else "Game& g, int card"
    hook("cards", name, prefix + (", " + suffix if suffix else ""), result, body)


hook("cards", "upgrade", "Card& card", selector="card.id")
hook("cards", "constructCard", "const Game& g, Card& card", selector="card.id")
hook("cards", "baseBlock", "const Card& card", "int", "const CardDef& d = card_def(card.id); return d.block + card.upgrades * d.upgrade_block + card.base_block_adjust;", selector="card.id")
card("use", "int target")
for name in ("tookDamage", "didDiscard", "onMoveToDiscard", "triggerWhenDrawn",
             "triggerWhenCopied", "triggerOnEndOfTurnForPlayingCard", "triggerOnManualDiscard", "triggerOnScry",
             "triggerAtStartOfTurn", "atTurnStart", "atTurnStartPreDraw", "onChoseThisOption", "onRetained",
             "triggerOnExhaust"):
    card(name)
card("triggerOnEndOfPlayerTurn", body="if (g.cards[card].ethereal) { Effect e{}; e.kind = EffectKind::ExhaustCard; action_initialize_state(e); e.card = card; add_top(g, e); }")
for name in ("triggerOnOtherCardPlayed", "triggerOnCardPlayed"):
    card(name, "int other_card")
card("switchedStance")
hook("cards", "onRemoveFromMasterDeck", "Game& g, Card& card", selector="card.id")
for name in ("triggerExhaustedCardsOnStanceChange",):
    card(name, "int stance")
card("triggerOnGainEnergy", "int amount, bool due_to_card")
card("onPlayCard", "int other_card, int target")
card("applyPowers", body="apply_card_powers(g, card);")
card("applyPowersToBlock", body="apply_card_block(g, card);")
card("calculateCardDamage", "int target", body="calculate_card_damage(g, card, target);")
card("resetAttributes", body="reset_attributes(g.cards[card]);")
card("clearPowers", body="card_resetAttributes(g, card);")
card("canUpgrade", result="bool", body="const Card& value = get_card(g, card); const CardType type = card_data(value.id).type; return type != CardType::Curse && type != CardType::Status && value.upgrades == 0;", const=True)
card("hasEnoughEnergy", result="bool", body="return card_has_enough_energy_base(g, card);", const=True)
card("freeToPlay", result="bool", body="return card_free_to_play_base(g, card);", const=True)
card("cardPlayable", "int target", "bool", "return !((g.cards[card].target == CardTarget::Enemy || g.cards[card].target == CardTarget::SelfAndEnemy) && target >= 0 && g.monsters[target].creature.dying) && !monsters_basically_dead(g);", True)
card("canPlay", "int other_card", "bool", "return true;", True)
card("canUse", "int target", "bool", "return card_can_use_base(g, card, target);", True)
card("makeStatEquivalentCopy", result="Card", body="return stat_equivalent_copy(g, card);")
card("makeSameInstanceOf", result="Card", body="Card copy = card_makeStatEquivalentCopy(g, card); copy.uuid = get_card(g, card).uuid; return copy;")
card("makeCopy", result="Card", body="return make_card(g, get_card(g, card).id);")

for name in ("use",):
    hook("potions", name, "Game& g, int slot, int target")
hook("potions", "canDiscard", "const Game& g, int slot", "bool", "return potion_can_discard_base(g, slot);")
hook("potions", "canUse", "const Game& g, int slot", "bool", "return potion_can_use_base(g, slot);")
hook("potions", "constructPotion", "const Game& g, PotionState& state, PotionId id", selector="id")
hook("potions", "potency", "const Game& g, int slot", "int", "return potion_data(g.run.potions[slot]).potency;")
for name in ("takeTurn", "usePreBattleAction", "useUniversalPreBattleAction", "onDamage", "deathReact",
             "escapeNext", "escape", "init", "applyPowers", "createIntent", "rollMove",
             "onBossVictoryLogic", "onFinalBossVictoryLogic"):
    body = {"onBossVictoryLogic": "g.death_wait_bound[owner] += 90; if (g.room_event_class == 0) ++g.run.boss_count;", "applyPowers": "monster_apply_powers(g, owner);", "createIntent": "create_monster_intent(g, owner);",
            "rollMove": "get_monster_move(g, owner, g.rng.ai.random(99));", "init": "monster_rollMove(g, owner);",
            "escape": "g.monsters[owner].creature.escaped = true; g.monsters[owner].creature.flip_horizontal = true;", "escapeNext": "g.monsters[owner].escape_next = true;"}.get(name, "")
    hook("monsters", name, "Game& g, int owner", body=body)
hook("monsters", "getMove", "Game& g, int owner, int num")
hook("monsters", "changeState", "Game& g, int owner, const char* state")
hook("monsters", "heal", "Game& g, int owner, int amount", body="heal_creature(g, owner, amount);")
hook("monsters", "damage", "Game& g, int owner, const DamageInfo& info", body="monster_damage_base(g, owner, info);")
hook("monsters", "die", "Game& g, int owner, bool trigger_relics = true", body="monster_die_base(g, owner, trigger_relics);")
hook("monsters", "setMove", "Game& g, int owner, int move, Intent intent, int damage, int hits", body="set_monster_move(g, owner, move, intent, damage, hits);")
hook("monsters", "construct", "Game& g, MonsterId id, int hp_override", "Monster", selector="id")
hook("encounters", "build", "Game& g, EncounterId id")
hook("events", "canSpawn", "const Game& g, EventId id", "bool", "return true;", selector="id")
hook("events", "choices", "const Game& g, EventChoices& choices", body="event_dialog_choices(g, choices);")
hook("events", "enter", "Game& g", body="SPIRE_ASSERT(false);")
hook("events", "choose", "Game& g, int option", body="SPIRE_ASSERT(false);")
hook("events", "update", "Game& g")
hook("events", "reopen", "Game& g")
hook("events", "referencesMasterCard", "const Game& g, uint32_t uuid", "bool", "return false;")
hook("actions", "prepare", "Game& g, Effect& e")
hook("actions", "execute", "Game& g, Effect& e")
hook("actions", "release", "Game& g, Effect& e")


def parameters(signature):
    return [part.strip() for part in signature.split(",") if part.strip()]


def arg_name(param):
    return re.search(r"(\w+)\s*$", param.split("=")[0].strip())[1]


def generate(root, output):
    catalogs = {}
    states = {}
    pattern = re.compile(r'SPIRE_UNIT\((\w+),\s*"([^"]+)",\s*"([^"]+)",\s*(\w+)\)')
    for kind in KINDS:
        catalog = root / "core" / kind / "catalog.def"
        catalogs[kind] = [pattern.fullmatch(line).groups() for line in catalog.read_text().splitlines() if line.strip() and not line.lstrip().startswith("//")]
    for kind, entries in catalogs.items():
        states[kind] = {}
        for unit, _, path, typename in entries:
            match = re.search(r"SPIRE_STATE\(\s*" + typename + r"\s*,(.*?)\)", (root / path).with_suffix(".inc").read_text(), re.S)
            if match:
                states[kind][unit] = (typename, match[1].strip())
    ids = ['#pragma once', '#include "core/fixed.hpp"', '#include "core/types.hpp"', 'namespace spire {']
    ids.append('struct UnitStatics {')
    for entries in catalogs.values():
        for _, _, path, typename in entries:
            match = re.search(r'SPIRE_STATIC_STATE\(\s*' + typename + r'\s*,(.*?)\)', (root / path).with_suffix(".inc").read_text(), re.S)
            if match:
                ids.append(f'    struct {{ {match[1].strip()} }} {typename}{{}};')
    ids.append('};')

    for kind, (enum, _, _, _) in KINDS.items():
        ids.append(f'enum class {enum} : {"uint8_t" if len(catalogs[kind]) < 256 else "uint16_t"} {{ ' + ', '.join(row[0] for row in catalogs[kind]) + ', Count };')
    declarations = ['#pragma once', 'namespace spire {']
    bases = ['#pragma once', 'namespace spire {']
    content = ['#pragma once', '#include "core/hooks.hpp"']
    for entries in catalogs.values():
        content += [f'#include "{Path(path).with_suffix(".hpp")}"' for _, _, path, _ in entries]
    content.append('namespace spire {')
    content.append('constexpr ActionData default_action_data{};')
    inventory = json.loads((root / 'inventory/units.json').read_text())['units']
    events = [unit for unit in inventory if unit['kind'] == 'event']
    ids.append('enum class EventKey : uint8_t { ' + ', '.join(unit['java_class'].rsplit('.', 1)[1] for unit in events) + ', Count };')
    declarations.append('SPIRE_FN inline const char* event_key_name(EventKey key);')
    content.append('SPIRE_FN inline const char* event_key_name(EventKey key) { switch (key) {')
    for unit in events:
        name = unit['java_class'].rsplit('.', 1)[1]
        content.append(f'case EventKey::{name}: return ' + json.dumps(unit['game_id']) + ';')
    content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    declarations.append('SPIRE_FN inline EventId event_id(EventKey key);')
    content.append('SPIRE_FN inline EventId event_id(EventKey key) { switch (key) {')
    for unit in events:
        name = unit['java_class'].rsplit('.', 1)[1]
        content.append(f'case EventKey::{name}: return EventId::{name};')
    content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    declarations += ['SPIRE_FN inline CardRarity card_rarity(CardId id);',
                     'SPIRE_FN inline int card_sort_order(CardId id);']
    card_facts = {unit['game_id']: unit['facts'] for unit in inventory if unit['kind'] == 'card'}
    content.append('struct RewardCardPoolEntry { CardRarity rarity; CardId native; CardType type; bool healing; };')
    content.append('constexpr RewardCardPoolEntry library_reward_cards[] = {')
    native_cards = {row[1]: row[0] for row in catalogs['cards']}
    for game_id, facts in sorted(card_facts.items()):
        if facts['rarity'] in ('COMMON', 'UNCOMMON', 'RARE') and facts['type'] not in ('CURSE', 'STATUS'):
            content.append('{CardRarity::' + facts['rarity'].title() + ', CardId::' + native_cards[game_id] + ', CardType::' + facts['type'].title() + ', ' + str('HEALING' in facts['tags']).lower() + '},')
    content.append('};')
    ranks = {game_id: rank for rank, game_id in enumerate(sorted(row[1] for row in catalogs['cards']))}
    for name, result in [('card_rarity', 'CardRarity'), ('card_sort_order', 'int')]:
        content.append(f'SPIRE_FN inline {result} {name}(CardId id) {{ switch (id) {{')
        for unit, game_id, _, _ in catalogs['cards']:
            value = ('CardRarity::' + card_facts[game_id]['rarity'].title()) if name == 'card_rarity' else str(ranks[game_id])
            content.append(f'case CardId::{unit}: return {value};')
        content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    declarations.append('SPIRE_FN inline bool card_colorless(CardId id);')
    content.append('SPIRE_FN inline bool card_colorless(CardId id) { switch (id) {')
    for unit, game_id, _, _ in catalogs['cards']:
        content.append(f'case CardId::{unit}: return ' + ('true' if card_facts[game_id]['color'] == 'COLORLESS' else 'false') + ';')
    content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    declarations.append('SPIRE_FN inline int card_name_order(const Card& card);')
    content.append('SPIRE_FN inline int card_name_order(const Card& card) { switch (card.id) {')
    for unit, game_id, _, _ in catalogs['cards']:
        first, second = card_facts[game_id]['name_order']
        content.append(f'case CardId::{unit}: return card.upgrades ? {second} : {first};')
    content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    relic_inventory = [unit for unit in inventory if unit['kind'] == 'relic']
    ids.append('enum class RelicKey : uint8_t { ' + ', '.join(unit['java_class'].rsplit('.', 1)[1] for unit in relic_inventory) + ', Count };')
    native_relics = {row[1]: row[0] for row in catalogs['relics']}
    declarations.append('SPIRE_FN inline RelicId relic_id(RelicKey key);')
    content.append('constexpr RelicId relic_key_ids[] = {')
    for unit in relic_inventory:
        content.append('RelicId::' + native_relics[unit['game_id']] + ',')
    content.append('};')
    content.append('SPIRE_FN inline RelicId relic_id(RelicKey key) { SPIRE_ASSERT(key < RelicKey::Count); return relic_key_ids[static_cast<int>(key)]; }')
    declarations.append('SPIRE_FN inline const char* relic_key_name(RelicKey key);')
    content.append('SPIRE_FN inline const char* relic_key_name(RelicKey key) { switch (key) {')
    for unit in relic_inventory:
        name = unit['java_class'].rsplit('.', 1)[1]
        content.append(f'case RelicKey::{name}: return ' + json.dumps(unit['game_id']) + ';')
    content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    declarations.append('SPIRE_FN inline bool relic_can_spawn_id(const Game& g, RelicId id);')
    content.append('SPIRE_FN inline bool relic_can_spawn_id(const Game& g, RelicId id) { switch (id) {')
    for unit, _, _, typename in catalogs['relics']:
        content.append(f'case RelicId::{unit}: return units::relics::{typename}::canSpawn(g, -1);')
    content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    ordered_relics = []
    for pool in ('shared', 'green'):
        entries = sorted((u for u in relic_inventory if u['facts']['pool'] == pool), key=lambda u: u['facts']['library_order'])
        capacity = 16
        while len(entries) > capacity * 3 // 4:
            capacity *= 2
        def bucket(unit):
            value = 0
            for char in unit['game_id']:
                value = (value * 31 + ord(char)) & 0xffffffff
            return (value ^ (value >> 16)) & (capacity - 1)
        ordered_relics.extend(sorted(entries, key=bucket))
    for tier in ('COMMON', 'UNCOMMON', 'RARE', 'SHOP', 'BOSS'):
        content.append(f'constexpr RelicKey initial_{tier.lower()}_relic_pool[] = {{')
        for unit in ordered_relics:
            if unit['facts']['tier'] == tier:
                content.append('RelicKey::' + unit['java_class'].rsplit('.', 1)[1] + ',')
        content.append('};')
    potion_facts = {unit['game_id']: unit['facts'] for unit in inventory if unit['kind'] == 'potion'}
    potion_pool = next(unit['facts']['silent_pool'] for unit in inventory if unit['key'] == 'rule:helpers.PotionHelper')
    potion_types = {game_id: (unit, typename) for unit, game_id, _, typename in catalogs['potions']}
    content.append('struct PotionPoolEntry { PotionId id; int rarity; };')
    content.append('constexpr PotionPoolEntry silent_potion_pool[] = {')
    for game_id in potion_pool:
        rarity = {'COMMON': 0, 'UNCOMMON': 1, 'RARE': 2}[potion_facts[game_id]['rarity']]
        unit, typename = potion_types[game_id]
        content.append('{PotionId::' + unit + ', ' + str(rarity) + '},')
    content.append('};')
    for kind, (enum, base, prefix, selector) in KINDS.items():
        entries = catalogs[kind]
        state_type = {"actions": "Effect", "encounters": "Encounter"}.get(kind, prefix.capitalize()) + "State"
        ids.append(f'union {state_type} {{')
        for typename, fields in states[kind].values():
            ids.append(f'    struct {typename}Fields {{ {fields} }};')
            ids.append(f'    static_assert(__is_trivially_constructible({typename}Fields), "Unit state fields need trivial default construction");')
        size, alignment = '1', '1'
        for typename, _ in states[kind].values():
            size = f'max({size}, sizeof({typename}Fields))'
            alignment = f'max({alignment}, alignof({typename}Fields))'
        ids.append(f'    using Word = StateWord<{alignment}>::type;')
        ids.append(f'    Word empty[({size} + sizeof(Word) - 1) / sizeof(Word)];')
        for typename, _ in states[kind].values():
            ids.append(f'    {typename}Fields {typename};')
        ids.append(f'    SPIRE_FN constexpr {state_type}() : empty{{}} {{}}')
        ids.append('};')
        budget = {"cards": 8, "powers": 8, "relics": 8, "potions": 8, "monsters": 32, "encounters": 16, "events": 64, "actions": 48}[kind]
        ids.append(f'static_assert(sizeof({state_type}) <= {budget}, "{state_type} exceeds its payload budget; use an arena index for larger state (docs/porting.md)");')
        if kind in ("cards", "powers", "relics", "monsters", "actions", "events"):
            entity = {"actions": "Effect"}.get(kind, prefix.capitalize())
            discriminator = "value.kind" if kind == "actions" else "value.id"
            declarations.append(f'SPIRE_FN inline void {prefix}_initialize_state({entity}& value);')
            content.append(f'SPIRE_FN inline void {prefix}_initialize_state({entity}& value) {{ switch ({discriminator}) {{')
            for unit, _, _, typename in entries:
                state_path = 'value.payload.state' if kind == 'actions' else 'value.state'
                assign = f'{state_path}.{typename} = {{}};' if unit in states[kind] else ''
                content.append(f'case {enum}::{unit}: {assign} ' + ('relic_initializeState(value); ' if kind == 'relics' else '') + 'return;')
            content.append('default: SPIRE_ASSERT(false); } }')
        if kind in ("cards", "powers", "relics", "monsters", "actions", "events"):
            content.append(f'template<class Visitor> SPIRE_FN inline void {prefix}_visit_state(const {entity}& value, Visitor& visitor) {{ (void)visitor; switch ({discriminator}) {{')
            for unit, _, _, typename in entries:
                content.append(f'case {enum}::{unit}:')
                if unit in states[kind]:
                    for field in states[kind][unit][1].split(';'):
                        if not field.strip():
                            continue
                        member = re.search(r'(\w+)\s*(?:\[[^]]+\])?\s*$', field.strip())[1]
                        state_path = 'value.payload.state' if kind == 'actions' else 'value.state'
                        content.append(f'visitor("{member}", {state_path}.{typename}.{member});')
                content.append('return;')
            content.append('default: SPIRE_ASSERT(false); } }')

        if kind in ("potions", "encounters"):
            params = "Game& g, int slot" if kind == "potions" else "Game& g"
            selector_state = "g.run.potions[slot]" if kind == "potions" else "g.encounter"
            value_state = "g.run.potion_state[slot]" if kind == "potions" else "g.encounter_state"
            declarations.append(f'SPIRE_FN inline void {prefix}_initialize_state({params});')
            content.append(f'SPIRE_FN inline void {prefix}_initialize_state({params}) {{ switch ({selector_state}) {{')
            for unit, _, _, typename in entries:
                assign = f'{value_state}.{typename} = {{}};' if unit in states[kind] else ''
                content.append(f'case {enum}::{unit}: {assign} return;')
            content.append('default: SPIRE_ASSERT(false); } }')
            params = params.replace("Game&", "const Game&")
            content.append(f'template<class Visitor> SPIRE_FN inline void {prefix}_visit_state({params}, Visitor& visitor) {{ (void)visitor; switch ({selector_state}) {{')
            for unit, _, _, typename in entries:
                content.append(f'case {enum}::{unit}:')
                if unit in states[kind]:
                    for field in states[kind][unit][1].split(';'):
                        if not field.strip():
                            continue
                        member = re.search(r'(\w+)\s*(?:\[[^]]+\])?\s*$', field.strip())[1]
                        content.append(f'visitor("{member}", {value_state}.{typename}.{member});')
                content.append('return;')
            content.append('default: SPIRE_ASSERT(false); } }')
        if kind in ("cards", "powers", "relics", "potions", "monsters", "actions", "events"):
            datatype = {"cards": "CardDef", "powers": "PowerData", "relics": "RelicData", "potions": "PotionData", "monsters": "MonsterData", "actions": "ActionData", "events": "EventData"}[kind]
            declarations.append(f'SPIRE_FN inline const {datatype}& {prefix}_data({enum} id);')
            content.append(f'SPIRE_FN inline const {datatype}& {prefix}_data({enum} id) {{ switch (id) {{')
            for unit, _, _, typename in entries:
                source = (root / next(path for key, _, path, _ in entries if key == unit)).with_suffix('.inc').read_text()
                body = re.search(r'\bstruct\s+' + typename + r'\s*[^{}]*\{', source).end() - 1
                own = source[body:close(masked(source), body)]
                if kind == 'actions' and not re.search(r'\bdata\s*\(', own):
                    content.append(f'case {enum}::{unit}: return default_action_data;')
                else:
                    content.append(f'case {enum}::{unit}: return units::{kind}::{typename}_data;')
            content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
        if base:
            bases.append(f'struct {base} {{')
            if kind == "actions":
                bases.append('    static SPIRE_FN constexpr ActionData data() { return {}; }')
        filters = set()
        for name, signature, result, body, custom_selector in HOOKS[kind]:
            params = parameters(signature)
            plain = ', '.join(param.split('=')[0].strip() for param in params)
            args = [arg_name(param) for param in params]
            declared = f'SPIRE_FN inline {result} {prefix}_{name}({signature});'
            declarations.append(declared)
            if base and name != "construct" and not (kind == "monsters" and name in ("getMove", "takeTurn")) and not (kind == "actions" and name == "execute"):
                unused = ' '.join(f'(void){arg};' for arg in args if not re.search(r'\b' + arg + r'\b', body))
                bases.append(f'    static SPIRE_FN {result} {name}({signature}) {{ {unused} {body} }}')
            discriminator = custom_selector or selector
            dispatch = []
            call_args = list(args)
            if name == 'construct' or kind == 'encounters':
                call_args.remove('id')
            inherited = []
            for unit, _, path, typename in entries:
                source = (root / path).with_suffix(".inc").read_text()
                if base and re.search(r'\bstruct\s+' + typename + r'\s*:\s*' + base + r'\s*\{', source) and not re.search(r'\b' + name + r'\s*\(', source):
                    inherited.append(unit)
                    continue
                invocation = f'units::{kind}::{typename}::{name}({", ".join(call_args)})'
                dispatch.append(f'case {enum}::{unit}: return {invocation};')
            identity = kind in ('powers', 'relics') and result == 'float' and body == 'return value;' and signature.startswith('const Game&')
            inactive = (result == 'void' and not body) or (result == 'bool' and body == 'return true;')
            if name not in filters and kind in ('cards', 'powers', 'relics') and (identity or inactive):
                filters.add(name)
                end = {'powers': '255', 'relics': 'g.run.relics.size', 'cards': 'pile.size'}[kind]
                extra = ', const Pile& pile' if kind == 'cards' else ''
                declarations.append(f'SPIRE_FN inline int next_{prefix}_{name}(const Game& g{extra}, int index);')
                content.append(f'SPIRE_FN inline int next_{prefix}_{name}(const Game& g{extra}, int index) {{')
                if len(inherited) == len(entries):
                    content.append(f'(void)g; (void)index; return {end}; }}')
                else:
                    content.append('constexpr bool active[] = {' + ', '.join('false' if unit in inherited else 'true' for unit, _, _, _ in entries) + '};')
                    advance = 'g.power_objects[index].next' if kind == 'powers' else 'index + 1'
                    object_id = {'powers': 'g.power_objects[index].id', 'relics': 'g.run.relics[index].id', 'cards': 'g.cards[pile[index]].id'}[kind]
                    content.append(f'for (; index != {end}; index = {advance}) {{')
                    content.append(f'const {enum} id = {object_id}; SPIRE_ASSERT(id < {enum}::Count);')
                    content.append('if (active[static_cast<int>(id)]) return index;')
                    content.append(f'}} return {end}; }}')
            if len(inherited) == len(entries):
                content.append(f'SPIRE_FN inline {result} {prefix}_{name}({plain}) {{ return {base}::{name}({", ".join(call_args)}); }}')
                continue
            content.append(f'SPIRE_FN inline {result} {prefix}_{name}({plain}) {{ SPIRE_ASSERT({discriminator} < {enum}::Count); switch ({discriminator}) {{')
            content.extend(dispatch)
            if inherited:
                content.append(f'default: return {base}::{name}({", ".join(call_args)}); }} }}')
            else:
                content.append('default: SPIRE_ASSERT(false); }' + (' __builtin_unreachable();' if result != 'void' else '') + ' }')
        if base:
            bases.append('};')
    for lines in (ids, declarations, bases, content):
        lines.append('}')
    output.mkdir(parents=True, exist_ok=True)
    for filename, lines in (("ids.hpp", ids), ("declarations.hpp", declarations)):
        write(output / filename, ('\n'.join(lines) + '\n').replace('SPIRE_FN inline', 'SPIRE_FN'))
    emit(root, output, {"bases.hpp": '\n'.join(bases) + '\n', "content.hpp": '\n'.join(content) + '\n'})



if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
