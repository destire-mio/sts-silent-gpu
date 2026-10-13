"""Compile unit catalogs into device-safe IDs, base hooks and switch dispatch."""
import argparse
import json
import re
from pathlib import Path

KINDS = {
    "cards": ("CardId", "CardHooks", "card", "get_card(g, card).id"),
    "powers": ("PowerId", "PowerHooks", "power", "p.id"),
    "relics": ("RelicId", "RelicHooks", "relic", "g.run.relics[relic].id"),
    "potions": ("PotionId", "PotionHooks", "potion", "g.run.potions[slot]"),
    "monsters": ("MonsterId", "MonsterHooks", "monster", "g.monsters[owner].id"),
    "encounters": ("EncounterId", None, "encounter", "id"),
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


for name in ("onGainGold", "onLoseGold", "onSpendGold", "onEquip", "onUnequip", "atPreBattle",
             "atBattleStart", "atBattleStartPreDraw", "atTurnStart", "atTurnStartPostDraw",
             "onPlayerEndTurn", "onBloodied", "onNotBloodied", "onManualDiscard", "onVictory",
             "onMeditate", "onEnergyRecharge", "onRest", "onRitual", "onEnterRestRoom", "onRefreshHand",
             "onShuffle", "onSmith", "onDrawOrDiscard", "onMasterDeckChange", "onUsePotion"):
    relic(name)
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
relic("addCampfireOption", "int options")


def card(name, suffix="", result="void", body="", const=False):
    prefix = "const Game& g, int card" if const else "Game& g, int card"
    hook("cards", name, prefix + (", " + suffix if suffix else ""), result, body)


hook("cards", "upgrade", "Card& card", selector="card.id")
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
card("canUpgrade", result="bool", body="const CardType type = card_data(g.cards[card].id).type; return type != CardType::Curse && type != CardType::Status && g.cards[card].upgrades == 0;", const=True)
card("hasEnoughEnergy", result="bool", body="return card_has_enough_energy_base(g, card);", const=True)
card("freeToPlay", result="bool", body="return card_free_to_play_base(g, card);", const=True)
card("cardPlayable", "int target", "bool", "return !(card_data(g.cards[card].id).target == CardTarget::Enemy && target >= 0 && g.monsters[target].creature.dying) && !monsters_basically_dead(g);", True)
card("canPlay", "int other_card", "bool", "return true;", True)
card("canUse", "int target", "bool", "return card_can_use_base(g, card, target);", True)
card("makeStatEquivalentCopy", result="Card", body="return stat_equivalent_copy(g, card);", const=True)
card("makeSameInstanceOf", result="Card", body="Card copy = card_makeStatEquivalentCopy(g, card); copy.uuid = g.cards[card].uuid; return copy;", const=True)
card("makeCopy", result="Card", body="return make_card(g.cards[card].id);", const=True)

for name in ("use",):
    hook("potions", name, "Game& g, int slot, int target")
hook("potions", "canUse", "const Game& g, int slot", "bool", "return true;")
for name in ("takeTurn", "usePreBattleAction", "useUniversalPreBattleAction", "onDamage", "deathReact",
             "escapeNext", "escape", "init", "applyPowers", "createIntent", "rollMove",
             "onBossVictoryLogic", "onFinalBossVictoryLogic"):
    body = {"applyPowers": "monster_apply_powers(g, owner);", "createIntent": "create_monster_intent(g, owner);",
            "rollMove": "get_monster_move(g, owner, g.rng.ai.random(99));", "init": "monster_rollMove(g, owner);",
            "escape": "g.monsters[owner].creature.escaped = true; g.monsters[owner].creature.flip_horizontal = true;", "escapeNext": "g.monsters[owner].escape_next = true;"}.get(name, "")
    hook("monsters", name, "Game& g, int owner", body=body)
hook("monsters", "stolenGold", "const Game& g, int owner", "int", body="return 0;")
hook("monsters", "split", "Game& g, int owner", body="SPIRE_ASSERT(false);")
hook("monsters", "getMove", "Game& g, int owner, int num")
hook("monsters", "changeState", "Game& g, int owner, const char* state")
hook("monsters", "heal", "Game& g, int owner, int amount", body="heal_creature(g, owner, amount);")
hook("monsters", "damage", "Game& g, int owner, const DamageInfo& info", body="monster_damage_base(g, owner, info);")
hook("monsters", "die", "Game& g, int owner, bool trigger_relics = true", body="monster_die_base(g, owner, trigger_relics);")
hook("monsters", "setMove", "Game& g, int owner, int move, Intent intent, int damage, int hits", body="set_monster_move(g, owner, move, intent, damage, hits);")
hook("monsters", "construct", "Game& g, MonsterId id, int hp_override", "Monster", selector="id")
hook("encounters", "build", "Game& g, EncounterId id")
hook("actions", "prepare", "Game& g, Effect& e")
hook("actions", "execute", "Game& g, Effect& e")


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
            match = re.search(r"SPIRE_STATE\(\s*" + typename + r"\s*,(.*?)\)", (root / path).read_text(), re.S)
            if match:
                states[kind][unit] = (typename, match[1].strip())
    ids = ['#pragma once', '#include "core/fixed.hpp"', '#include "core/types.hpp"', 'namespace spire {']
    for kind, (enum, _, _, _) in KINDS.items():
        ids.append(f'enum class {enum} : uint16_t {{ ' + ', '.join(row[0] for row in catalogs[kind]) + ', Count };')
    declarations = ['#pragma once', 'namespace spire {']
    bases = ['#pragma once', 'namespace spire {']
    content = ['#pragma once', '#include "core/hooks.hpp"']
    for entries in catalogs.values():
        content += [f'#include "{path}"' for _, _, path, _ in entries]
    content.append('namespace spire {')
    inventory = json.loads((root / 'inventory/units.json').read_text())['units']
    declarations += ['SPIRE_FN inline CardRarity card_rarity(CardId id);',
                     'SPIRE_FN inline int card_sort_order(CardId id);']
    card_facts = {unit['game_id']: unit['facts'] for unit in inventory if unit['kind'] == 'card'}
    ranks = {game_id: rank for rank, game_id in enumerate(sorted(row[1] for row in catalogs['cards']))}
    for name, result in [('card_rarity', 'CardRarity'), ('card_sort_order', 'int')]:
        content.append(f'SPIRE_FN inline {result} {name}(CardId id) {{ switch (id) {{')
        for unit, game_id, _, _ in catalogs['cards']:
            value = ('CardRarity::' + card_facts[game_id]['rarity'].title()) if name == 'card_rarity' else str(ranks[game_id])
            content.append(f'case CardId::{unit}: return {value};')
        content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    for kind in ('powers', 'relics'):
        enum, _, prefix, _ = KINDS[kind]
        declarations.append(f'SPIRE_FN inline bool {prefix}_matches_id({enum} id, const char* java_id);')
        content.append(f'SPIRE_FN inline bool {prefix}_matches_id({enum} id, const char* java_id) {{ switch (id) {{')
        for unit, java_id, _, _ in catalogs[kind]:
            content.append(f'case {enum}::{unit}: return same_text(java_id, "{java_id}");')
        content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
    for kind, (enum, base, prefix, selector) in KINDS.items():
        entries = catalogs[kind]
        state_type = {"actions": "Effect", "encounters": "Encounter"}.get(kind, prefix.capitalize()) + "State"
        ids.append(f'union {state_type} {{')
        ids.append('    uint8_t empty;')
        for typename, fields in states[kind].values():
            ids.append(f'    struct {{ {fields} }} {typename};')
            ids.append(f'    static_assert(__is_trivially_constructible(decltype({typename})), "Unit state fields need trivial default construction");')
        ids.append(f'    SPIRE_FN constexpr {state_type}() : empty{{}} {{}}')
        ids.append('};')
        budget = {"cards": 8, "powers": 8, "relics": 8, "potions": 8, "monsters": 32, "encounters": 16, "actions": 48}[kind]
        ids.append(f'static_assert(sizeof({state_type}) <= {budget}, "{state_type} exceeds its payload budget; use an arena index for larger state (docs/porting.md)");')
        if kind in ("cards", "powers", "relics", "monsters", "actions"):
            entity = {"actions": "Effect"}.get(kind, prefix.capitalize())
            discriminator = "value.kind" if kind == "actions" else "value.id"
            declarations.append(f'SPIRE_FN inline void {prefix}_initialize_state({entity}& value);')
            content.append(f'SPIRE_FN inline void {prefix}_initialize_state({entity}& value) {{ switch ({discriminator}) {{')
            for unit, _, _, typename in entries:
                assign = f'value.state.{typename} = {{}};' if unit in states[kind] else ''
                content.append(f'case {enum}::{unit}: {assign} return;')
            content.append('default: SPIRE_ASSERT(false); } }')
        if kind in ("cards", "powers", "relics", "monsters", "actions"):
            content.append(f'template<class Visitor> SPIRE_FN inline void {prefix}_visit_state(const {entity}& value, Visitor& visitor) {{ (void)visitor; switch ({discriminator}) {{')
            for unit, _, _, typename in entries:
                content.append(f'case {enum}::{unit}:')
                if unit in states[kind]:
                    for field in states[kind][unit][1].split(';'):
                        if not field.strip():
                            continue
                        member = re.search(r'(\w+)\s*(?:\[[^]]+\])?\s*$', field.strip())[1]
                        content.append(f'visitor("{member}", value.state.{typename}.{member});')
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
        if kind in ("cards", "powers", "relics", "potions", "monsters", "actions"):
            datatype = {"cards": "CardDef", "powers": "PowerData", "relics": "RelicData", "potions": "PotionData", "monsters": "MonsterData", "actions": "ActionData"}[kind]
            declarations.append(f'SPIRE_FN inline {datatype} {prefix}_data({enum} id);')
            content.append(f'SPIRE_FN inline {datatype} {prefix}_data({enum} id) {{ switch (id) {{')
            for unit, _, _, typename in entries:
                content.append(f'case {enum}::{unit}: return units::{kind}::{typename}::data();')
            content.append('default: SPIRE_ASSERT(false); } __builtin_unreachable(); }')
        if base:
            bases.append(f'struct {base} {{')
            if kind == "actions":
                bases.append('    static SPIRE_FN constexpr ActionData data() { return {}; }')
        for name, signature, result, body, custom_selector in HOOKS[kind]:
            params = parameters(signature)
            plain = ', '.join(param.split('=')[0].strip() for param in params)
            args = [arg_name(param) for param in params]
            declared = f'SPIRE_FN inline {result} {prefix}_{name}({signature});'
            declarations.append(declared)
            if base and name != "construct" and not (kind == "monsters" and name in ("getMove", "takeTurn")) and not (kind == "actions" and name == "execute"):
                unused = ' '.join(f'(void){arg};' for arg in args if not re.search(r'\b' + arg + r'\b', body))
                bases.append(f'    static SPIRE_FN {result} {name}({signature}) {{ {unused} {body} }}')
            content.append(f'SPIRE_FN inline {result} {prefix}_{name}({plain}) {{ switch ({custom_selector or selector}) {{')
            call_args = list(args)
            if name == 'construct' or kind == 'encounters':
                call_args.remove('id')
            for unit, _, _, typename in entries:
                invocation = f'units::{kind}::{typename}::{name}({", ".join(call_args)})'
                content.append(f'case {enum}::{unit}: return {invocation};')
            content.append('default: SPIRE_ASSERT(false); }' + (' __builtin_unreachable();' if result != 'void' else '') + ' }')
        if base:
            bases.append('};')
    for lines in (ids, declarations, bases, content):
        lines.append('}')
    output.mkdir(parents=True, exist_ok=True)
    for filename, lines in (("ids.hpp", ids), ("bases.hpp", bases), ("declarations.hpp", declarations), ("content.hpp", content)):
        path = output / filename
        text = '\n'.join(lines) + '\n'
        if not path.exists() or path.read_text() != text:
            path.write_text(text)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
