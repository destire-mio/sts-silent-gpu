#pragma once
#include "card_data.hpp"

namespace spire {
SPIRE_FN inline int16_t& monster_base_damage(Game& g, Monster& m, int slot) {
    return slot < 3 ? m.damage[slot] : g.monster_damage_extra[m.damage_extra + slot - 3].base;
}
SPIRE_FN inline const int16_t& monster_base_damage(const Game& g, const Monster& m, int slot) {
    return slot < 3 ? m.damage[slot] : g.monster_damage_extra[m.damage_extra + slot - 3].base;
}
SPIRE_FN inline int16_t& monster_output_damage(Game& g, Monster& m, int slot) {
    return slot < 3 ? m.damage_output[slot] : g.monster_damage_extra[m.damage_extra + slot - 3].output;
}
SPIRE_FN inline const int16_t& monster_output_damage(const Game& g, const Monster& m, int slot) {
    return slot < 3 ? m.damage_output[slot] : g.monster_damage_extra[m.damage_extra + slot - 3].output;
}
SPIRE_FN inline bool same_id(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
SPIRE_FN inline bool card_is(CardId id, const char* name) {
    switch (id) {
#define SPIRE_UNIT(id, java_name, header, type) case CardId::id: return same_id(java_name, name);
#include "cards/catalog.def"
#undef SPIRE_UNIT
    default: SPIRE_ASSERT(false); }
    __builtin_unreachable();
}
SPIRE_FN inline bool monster_is(MonsterId id, const char* name) {
    switch (id) {
#define SPIRE_UNIT(id, java_name, header, type) case MonsterId::id: return same_id(java_name, name);
#include "monsters/catalog.def"
#undef SPIRE_UNIT
    default: SPIRE_ASSERT(false);
    }
    __builtin_unreachable();
}
SPIRE_FN inline CardId card_id(const char* name) {
    for (int i = 0; i < static_cast<int>(CardId::Count); ++i)
        if (card_is(static_cast<CardId>(i), name)) return static_cast<CardId>(i);
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline MonsterId monster_id(const char* name) {
#define SPIRE_UNIT(unit, id, path, type) if (same_id(name, id)) return MonsterId::unit;
#include "monsters/catalog.def"
#undef SPIRE_UNIT
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline int find_power(const Game& g, const Creature& c, const char* name) {
    for (int i = 0; i < c.powers.size; ++i) {
        const char* game_id = nullptr;
        switch (c.powers.at(g.power_objects, i).id) {
#define SPIRE_UNIT(id, java_name, header, type) case PowerId::id: game_id = java_name; break;
#include "powers/catalog.def"
#undef SPIRE_UNIT
        default: SPIRE_ASSERT(false); }
        if (same_id(game_id, name)) return i;
    }
    return -1;
}
SPIRE_FN inline bool has_power(const Game& g, const Creature& c, const char* name) { return find_power(g, c, name) >= 0; }
SPIRE_FN inline Card& get_card(Game& g, int index) {
    return index <= -2 ? g.run.master_deck[-index - 2] : g.cards[index];
}
SPIRE_FN inline const Card& get_card(const Game& g, int index) {
    return index <= -2 ? g.run.master_deck[-index - 2] : g.cards[index];
}
SPIRE_FN inline Creature& get_creature(Game& g, int target) {
    return target == player_target ? g.player.creature : g.monsters[target].creature;
}
SPIRE_FN inline const Creature& get_creature(const Game& g, int target) {
    return target == player_target ? g.player.creature : g.monsters[target].creature;
}
SPIRE_FN inline bool alive(const Creature& c) { return !c.dead && !c.dying && !c.escaped && !c.half_dead; }
SPIRE_FN inline int find_power(const Game& g, const Creature& c, PowerId id, int offset) {
    for (int i = 0; i < c.powers.size; ++i)
        if (c.powers.at(g.power_objects, i).id == id && c.powers.at(g.power_objects, i).id_offset == offset) return i;
    return -1;
}
SPIRE_FN inline bool has_power(const Game& g, const Creature& c, PowerId id) { return find_power(g, c, id) >= 0; }
SPIRE_FN inline int power_amount(const Game& g, const Creature& c, PowerId id) {
    const int index = find_power(g, c, id);
    return index < 0 ? 0 : c.powers.at(g.power_objects, index).amount;
}
SPIRE_FN inline bool monsters_basically_dead(const Game& g) {
    for (int i = 0; i < g.monster_order.size; ++i) {
        const Creature& c = g.monsters[g.monster_order[i]].creature;
        if (!c.dying && !c.escaped) return false;
    }
    return true;
}
SPIRE_FN inline const char* power_java_id(PowerId id) {
    switch (id) {
#define SPIRE_UNIT(unit, name, path, type) case PowerId::unit: return name;
#include "powers/catalog.def"
#undef SPIRE_UNIT
    default: SPIRE_ASSERT(false);
    }
    __builtin_unreachable();
}
SPIRE_FN inline PowerId power_id(const char* name) {
#define SPIRE_UNIT(unit, id, path, type) if (same_id(name, id)) return PowerId::unit;
#include "powers/catalog.def"
#undef SPIRE_UNIT
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline bool has_relic(const Game& g, const char* name) {
    for (int i = 0; i < g.run.relics.size; ++i) {
        switch (g.run.relics[i].id) {
#define SPIRE_UNIT(unit, id, path, type) case RelicId::unit: if (same_id(name, id)) return true; break;
#include "relics/catalog.def"
#undef SPIRE_UNIT
        default: SPIRE_ASSERT(false);
        }
    }
    return false;
}
SPIRE_FN inline int random_living_monster(Game& g) {
    if (monsters_basically_dead(g)) return no_target;
    List<int, monster_capacity> candidates{};
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int owner = g.monster_order[i];
        const Creature& monster = g.monsters[owner].creature;
        if (!monster.half_dead && !monster.dying && !monster.escaped) candidates.push_back(owner);
    }
    if (candidates.empty()) return no_target;
    return candidates[g.rng.card_random.random(0, candidates.size - 1)];
}
SPIRE_FN inline int hold_action(Game& g, const Effect& effect) {
    Effect prepared = effect;
    action_prepare(g, prepared);
    prepared.held = true;
    return g.action_objects.add(prepared);
}
SPIRE_FN inline void add_bottom(Game& g, const Effect& effect) {
    Effect prepared = effect;
    action_prepare(g, prepared);
    if (g.room_phase == RoomPhase::Combat)
        g.manager.actions.push_back(g.action_objects.add(prepared));
}
SPIRE_FN inline void add_top(Game& g, const Effect& effect) {
    Effect prepared = effect;
    action_prepare(g, prepared);
    if (g.room_phase == RoomPhase::Combat)
        g.manager.actions.push_front(g.action_objects.add(prepared));
}
SPIRE_FN inline void release_action(Game& g, int index) {
    action_release(g, g.action_objects[index]);
    g.action_objects.release(index);
}
SPIRE_FN inline void clear_actions(Game& g, Queue<uint8_t, action_capacity>& queue) {
    while (!queue.empty()) {
        const int index = queue.pop_front();
        if (!g.action_objects[index].held) release_action(g, index);
    }
}
SPIRE_FN inline float rear_damage_multiplier(const Game& g, int monster) {
    const Creature& player = g.player.creature;
    if (!((player.flip_horizontal && player.draw_x < g.monsters[monster].creature.draw_x) ||
          (!player.flip_horizontal && player.draw_x > g.monsters[monster].creature.draw_x))) return 1.0f;
    float multiplier = 1.0f;
    for (int i = 0; i < player.powers.size; ++i)
        multiplier *= power_data(player.powers.at(g.power_objects, i).id).rear_damage_multiplier;
    return multiplier;
}
SPIRE_FN inline int calculate_intent_damage(const Game& g, int monster, int base) {
    float value = static_cast<float>(base);
    const Powers& source = g.monsters[monster].creature.powers;
    const Powers& target = g.player.creature.powers;
    for (int i = 0; i < source.size; ++i) value = power_atDamageGive(g, source.at(g.power_objects, i), monster, value, DamageType::Normal);
    for (int i = 0; i < target.size; ++i) value = power_atDamageReceive(g, target.at(g.power_objects, i), player_target, value, DamageType::Normal);
    value = stance_damage_receive(g, value, DamageType::Normal);
    const float multiplier = rear_damage_multiplier(g, monster);
    if (multiplier != 1.0f) value = static_cast<int>(value * multiplier);
    for (int i = 0; i < source.size; ++i) value = power_atDamageFinalGive(g, source.at(g.power_objects, i), monster, value, DamageType::Normal);
    for (int i = 0; i < target.size; ++i) value = power_atDamageFinalReceive(g, target.at(g.power_objects, i), player_target, value, DamageType::Normal);
    return max(0, floor_int(value));
}
SPIRE_FN inline int calculate_damage(const Game& g, int source, int target, int base, DamageType type, int card) {
    float value = static_cast<float>(base);
    if (card >= 0) {
        for (int i = 0; i < g.run.relics.size; ++i) value = relic_atDamageModify(g, i, value, card);
    }
    if (source != no_target) {
        const Powers& powers = get_creature(g, source).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageGive(g, powers.at(g.power_objects, i), source, value, type, card);
    }
    if (source == player_target) value = stance_damage_give(g, value, type);
    if (target != no_target) {
        const Powers& powers = get_creature(g, target).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageReceive(g, powers.at(g.power_objects, i), target, value, type, card);
    }
    if (source >= 0) value = stance_damage_receive(g, value, type);
    if (source != no_target) {
        const Powers& powers = get_creature(g, source).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageFinalGive(g, powers.at(g.power_objects, i), source, value, type, card);
    }
    if (target != no_target) {
        const Powers& powers = get_creature(g, target).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageFinalReceive(g, powers.at(g.power_objects, i), target, value, type, card);
    }
    return max(0, floor_int(value));
}
SPIRE_FN inline int calculate_block(const Game& g, int base, int card) {
    float value = static_cast<float>(base);
    const Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlock(g, powers.at(g.power_objects, i), player_target, value, card);
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlockLast(g, powers.at(g.power_objects, i), player_target, value);
    return max(0, floor_int(value));
}
SPIRE_FN inline void apply_card_block(Game& g, int card) {
    Card& c = get_card(g, card);
    float value = static_cast<float>(card_baseBlock(c));
    const Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlock(g, powers.at(g.power_objects, i), player_target, value, card);
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlockLast(g, powers.at(g.power_objects, i), player_target, value);
    // AbstractCard.applyPowersToBlock compares before clamping negative block.
    c.block = max(0, floor_int(value));
}
SPIRE_FN inline void refresh_powers(Game& g) {
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    if (!g.orbs.slots.empty() && has_power(g, g.player.creature, PowerId::Focus))
        for (int i = 0; i < g.orbs.slots.size; ++i) if (g.orbs.slots[i] >= 0) apply_orb_focus(g, g.orbs.slots[i]);
    for (int i = 0; i < g.monster_order.size; ++i) monster_applyPowers(g, g.monster_order[i]);
}
SPIRE_FN inline void queue_monster_damage(Game& g, int source, int target, int slot, int steal, bool skip_wait) {
    Effect e = units::actions::Damage::from_shared_info(
        units::actions::Damage::monster_info(g, source, slot), target,
        units::actions::Damage::monster_identity(slot), skip_wait);
    e.extra = steal;
    add_bottom(g, e);
}
SPIRE_FN inline void queue_block(Game& g, int target, int amount) {
    Effect e{};
    e.kind = EffectKind::Block;
    e.source = target;
    e.target = target;
    e.amount = amount;
    add_bottom(g, e);
}
SPIRE_FN inline void queue_power(Game& g, int source, int target, PowerId id, int amount, bool monster_source, int extra, int card) {
    Power power{};
    power.id = id;
    power.amount = amount;
    power_initFromSource(g, power, target, monster_source);
    Effect e{};
    e.kind = EffectKind::ApplyPower;
    action_initialize_state(e);
    e.source = source;
    e.target = target;
    e.power = id;
    e.amount = amount;
    e.flag = power.just_applied;
    e.extra = extra;
    e.card = card;
    add_bottom(g, e);
}
SPIRE_FN inline void gain_block(Game& g, int target, int amount) {
    Creature& c = get_creature(g, target);
    if (c.dead || c.dying) return;
    float value = static_cast<float>(amount);
    if (target == player_target) {
        for (int i = 0; i < g.run.relics.size; ++i) value = static_cast<float>(relic_onPlayerGainedBlock(g, i, value));
        if (value > 0.0f) {
            for (int i = 0; i < c.powers.size; ++i) power_onGainedBlock(g, c.powers.at(g.power_objects, i), target, value);
        }
    }
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int owner = g.monster_order[i];
        Powers& powers = g.monsters[owner].creature.powers;
        for (int j = 0; j < powers.size; ++j) value = static_cast<float>(power_onPlayerGainedBlock(g, powers.at(g.power_objects, j), owner, value));
    }
    c.block = min(999, c.block + floor_int(value));
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
}
SPIRE_FN inline Power& power_instance(Game& g, uint32_t instance) {
    for (int i = 0; i < power_object_capacity; ++i)
        if (g.power_objects.contains(i) && g.power_objects[i].instance == instance) return g.power_objects[i];
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline const Power& power_instance(const Game& g, uint32_t instance) {
    for (int i = 0; i < power_object_capacity; ++i)
        if (g.power_objects.contains(i) && g.power_objects[i].instance == instance) return g.power_objects[i];
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline void remove_power(Game& g, int target, PowerId id, uint32_t instance) {
    Creature& c = get_creature(g, target);
    if (c.dying || c.half_dead || (target != player_target && c.escaped)) return;
    int index = find_power(g, c, id);
    if (instance != 0) {
        index = -1;
        for (int i = 0; i < c.powers.size; ++i) if (c.powers.at(g.power_objects, i).instance == instance) { index = i; break; }
    }
    if (index >= 0) {
        power_onRemove(g, c.powers.at(g.power_objects, index), target);
        c.powers.erase(g.power_objects, index);
        refresh_powers(g);
        for (int i = 0; i < g.orbs.slots.size; ++i)
            if (g.orbs.slots[i] >= 0) apply_orb_focus(g, g.orbs.slots[i]);
    }
}
SPIRE_FN inline bool keeps_block(const Game& g, int owner) {
    const Powers& powers = get_creature(g, owner).powers;
    for (int i = 0; i < powers.size; ++i) {
        const PowerData data = power_data(powers.at(g.power_objects, i).id);
        if (owner == player_target ? data.keep_player_block : data.keep_monster_block) return true;
    }
    return false;
}
SPIRE_FN inline void apply_prepared_power(Game& g, int source, int target, Power incoming, int amount) {
    if (target == no_target) return;
    const PowerId id = incoming.id;
    Creature& c = get_creature(g, target);
    if (c.dying || c.half_dead || (target != player_target && c.escaped)) return;
    const int existing = find_power(g, c, id, incoming.id_offset);
    if (!power_data(id).can_stack && existing >= 0) return;
    if (source != no_target) {
        Powers& powers = get_creature(g, source).powers;
        for (int i = 0; i < powers.size; ++i) power_onApplyPower(g, powers.at(g.power_objects, i), source, incoming, target, source);
    }
    if (target != player_target && (c.dying || c.half_dead || c.escaped)) return;
    if (power_isDebuff(incoming, target)) {
        for (int i = 0; i < c.powers.size; ++i) {
            if (!power_data(c.powers.at(g.power_objects, i).id).blocks_debuffs) continue;
            power_onSpecificTrigger(g, c.powers.at(g.power_objects, i), target);
            return;
        }
    }
    bool stacked = false;
    for (int i = 0; i < c.powers.size; ++i) {
        if (c.powers.at(g.power_objects, i).id != id || c.powers.at(g.power_objects, i).id_offset != incoming.id_offset || power_data(id).stacks_separately) continue;
        power_stackPower(g, c.powers.at(g.power_objects, i), target, amount);
        stacked = true;
        refresh_powers(g);
    }
    if (!stacked) {
        if (incoming.instance == 0) incoming.instance = next_power_identity(g);
        const int applied = g.power_objects.add(incoming);
        c.powers.push_back(g.power_objects, applied);
        // ApplyPowerAction.update sorts the entire list, including direct relic additions.
        for (int i = 1; i < c.powers.size; ++i) {
            const int object = c.powers.index(g.power_objects, i);
            int index = i;
            while (index > 0 && power_data(c.powers.at(g.power_objects, index - 1).id).priority >
                   power_data(g.power_objects[object].id).priority) --index;
            if (index != i) {
                c.powers.erase(g.power_objects, i);
                c.powers.insert(g.power_objects, index, object);
            }
        }
        power_onInitialApplication(g, g.power_objects[applied], target);
        refresh_powers(g);
    }
}
SPIRE_FN inline void apply_power(Game& g, int source, int target, PowerId id, int amount, bool just_applied, int extra, int card) {
    Power incoming{};
    incoming.id = id;
    incoming.amount = amount;
    incoming.just_applied = just_applied;
    power_constructPower(g, incoming, target, source, extra, card);
    incoming.instance = next_power_identity(g);
    apply_prepared_power(g, source, target, incoming, amount);
}
SPIRE_FN inline void reduce_power(Game& g, int target, PowerId id, int amount, int source) {
    Creature& c = get_creature(g, target);
    const int index = find_power(g, c, id);
    if (index < 0) return;
    if (amount >= c.powers.at(g.power_objects, index).amount) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = source;
        e.target = target;
        e.power = id;
        e.extra = c.powers.at(g.power_objects, index).instance;
        add_top(g, e);
    } else {
        power_reducePower(g, c.powers.at(g.power_objects, index), target, amount);
        refresh_powers(g);
    }
}
SPIRE_FN inline void at_end_of_round(Game& g, int target) {
    Powers& powers = get_creature(g, target).powers;
    for (int i = 0; i < powers.size; ++i) power_atEndOfRound(g, powers.at(g.power_objects, i), target);
}
SPIRE_FN inline void at_end_of_turn(Game& g, int target) {
    Powers& powers = get_creature(g, target).powers;
    for (int i = 0; i < powers.size; ++i) {
        if (target != player_target) power_atEndOfTurnPreEndTurnCards(g, powers.at(g.power_objects, i), target, false);
        power_atEndOfTurn(g, powers.at(g.power_objects, i), target, target == player_target);
    }
}
}
