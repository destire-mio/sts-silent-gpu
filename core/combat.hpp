#pragma once
#include "card_data.hpp"

namespace spire {
SPIRE_FN inline Creature& get_creature(Game& g, int target) {
    return target == player_target ? g.player.creature : g.monsters[target].creature;
}
SPIRE_FN inline const Creature& get_creature(const Game& g, int target) {
    return target == player_target ? g.player.creature : g.monsters[target].creature;
}
SPIRE_FN inline bool alive(const Creature& c) { return !c.dead && !c.dying && !c.escaped && !c.half_dead; }
SPIRE_FN inline int find_power(const Creature& c, PowerId id) {
    for (int i = 0; i < c.powers.size; ++i) if (c.powers[i].id == id) return i;
    return -1;
}
SPIRE_FN inline bool has_power(const Creature& c, PowerId id) { return find_power(c, id) >= 0; }
SPIRE_FN inline int power_amount(const Creature& c, PowerId id) {
    const int index = find_power(c, id);
    return index < 0 ? 0 : c.powers[index].amount;
}
SPIRE_FN inline bool monsters_basically_dead(const Game& g) {
    for (int i = 0; i < g.monster_order.size; ++i) {
        const Creature& c = g.monsters[g.monster_order[i]].creature;
        if (!c.dying && !c.escaped) return false;
    }
    return true;
}
SPIRE_FN inline void add_bottom(Game& g, const Effect& effect) {
    Effect prepared = effect;
    action_prepare(g, prepared);
    g.manager.actions.push_back(prepared);
}
SPIRE_FN inline void add_top(Game& g, const Effect& effect) {
    Effect prepared = effect;
    action_prepare(g, prepared);
    g.manager.actions.push_front(prepared);
}
SPIRE_FN inline int calculate_damage(const Game& g, int source, int target, int base, DamageType type, int card) {
    float value = static_cast<float>(base);
    if (card >= 0) {
        for (int i = 0; i < g.run.relics.size; ++i) value = relic_atDamageModify(g, i, value, card);
    }
    if (source != no_target) {
        const Powers& powers = get_creature(g, source).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageGive(g, powers[i], source, value, type, card);
    }
    if (target != no_target) {
        const Powers& powers = get_creature(g, target).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageReceive(g, powers[i], target, value, type, card);
    }
    if (source != no_target) {
        const Powers& powers = get_creature(g, source).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageFinalGive(g, powers[i], source, value, type, card);
    }
    if (target != no_target) {
        const Powers& powers = get_creature(g, target).powers;
        for (int i = 0; i < powers.size; ++i) value = power_atDamageFinalReceive(g, powers[i], target, value, type, card);
    }
    return max(0, floor_int(value));
}
SPIRE_FN inline int calculate_block(const Game& g, int base, int card) {
    float value = static_cast<float>(base);
    const Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlock(g, powers[i], player_target, value, card);
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlockLast(g, powers[i], player_target, value);
    return max(0, floor_int(value));
}
SPIRE_FN inline void apply_card_block(Game& g, int card) {
    Card& c = g.cards[card];
    float value = static_cast<float>(c.base_block);
    const Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlock(g, powers[i], player_target, value, card);
    for (int i = 0; i < powers.size; ++i) value = power_modifyBlockLast(g, powers[i], player_target, value);
    // AbstractCard.applyPowersToBlock compares before clamping negative block.
    c.block = max(0, floor_int(value));
}
SPIRE_FN inline void refresh_powers(Game& g) {
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    for (int i = 0; i < g.monster_order.size; ++i) monster_applyPowers(g, g.monster_order[i]);
}
SPIRE_FN inline void queue_damage(Game& g, int source, int target, int base, DamageType type, int steal) {
    Effect e{};
    e.kind = EffectKind::Damage;
    e.source = source;
    e.target = target;
    e.amount = calculate_damage(g, source, target, base, type);
    e.info_base = base;
    e.damage_type = type;
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
SPIRE_FN inline void queue_power(Game& g, int source, int target, PowerId id, int amount, bool monster_source) {
    if (monsters_basically_dead(g)) return;
    Power power{};
    power.id = id;
    power.amount = amount;
    power_initFromSource(g, power, target, monster_source);
    Effect e{};
    e.kind = EffectKind::ApplyPower;
    e.source = source;
    e.target = target;
    e.power = id;
    e.amount = amount;
    e.flag = power.just_applied;
    add_bottom(g, e);
}
SPIRE_FN inline void gain_block(Game& g, int target, int amount) {
    Creature& c = get_creature(g, target);
    if (c.dead || c.dying) return;
    float value = static_cast<float>(amount);
    if (target == player_target) {
        for (int i = 0; i < g.run.relics.size; ++i) value = static_cast<float>(relic_onPlayerGainedBlock(g, i, value));
        if (value > 0.0f) {
            for (int i = 0; i < c.powers.size; ++i) power_onGainedBlock(g, c.powers[i], target, value);
        }
    }
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int owner = g.monster_order[i];
        Powers& powers = g.monsters[owner].creature.powers;
        for (int j = 0; j < powers.size; ++j) value = static_cast<float>(power_onPlayerGainedBlock(g, powers[j], owner, value));
    }
    c.block = min(999, c.block + floor_int(value));
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
}
SPIRE_FN inline void remove_power(Game& g, int target, PowerId id) {
    Creature& c = get_creature(g, target);
    if (c.dying || c.half_dead || (target != player_target && c.escaped)) return;
    const int index = find_power(c, id);
    if (index >= 0) {
        power_onRemove(g, c.powers[index], target);
        c.powers.erase(index);
        refresh_powers(g);
    }
}
SPIRE_FN inline void apply_prepared_power(Game& g, int source, int target, Power incoming, int amount) {
    const PowerId id = incoming.id;
    Creature& c = get_creature(g, target);
    if (c.dying || c.half_dead || (target != player_target && c.escaped)) return;
    const int existing = find_power(c, id);
    if (!power_data(id).can_stack && existing >= 0) return;
    if (source != no_target) {
        Powers& powers = get_creature(g, source).powers;
        for (int i = 0; i < powers.size; ++i) power_onApplyPower(g, powers[i], source, incoming, target, source);
    }
    const int artifact = find_power(c, PowerId::Artifact);
    if (artifact >= 0 && power_isDebuff(incoming, target)) {
        power_onSpecificTrigger(g, c.powers[artifact], target);
        return;
    }
    if (existing >= 0) {
        power_stackPower(g, c.powers[existing], target, amount);
    } else {
        int index = c.powers.size;
        while (index > 0 && power_data(c.powers[index - 1].id).priority > power_data(id).priority) --index;
        c.powers.insert(index, incoming);
        power_onInitialApplication(g, c.powers[index], target);
    }
    refresh_powers(g);
}
SPIRE_FN inline void apply_power(Game& g, int source, int target, PowerId id, int amount, bool just_applied) {
    Power incoming{};
    incoming.id = id;
    incoming.amount = amount;
    incoming.just_applied = just_applied;
    power_initialize_state(incoming);
    power_initialPower(g, incoming, target);
    apply_prepared_power(g, source, target, incoming, amount);
}
SPIRE_FN inline void reduce_power(Game& g, int target, PowerId id, int amount) {
    Creature& c = get_creature(g, target);
    const int index = find_power(c, id);
    if (index < 0) return;
    if (amount >= c.powers[index].amount) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.target = target;
        e.power = id;
        add_top(g, e);
    } else {
        power_reducePower(g, c.powers[index], target, amount);
        refresh_powers(g);
    }
}
SPIRE_FN inline void at_end_of_round(Game& g, int target) {
    Powers& powers = get_creature(g, target).powers;
    for (int i = 0; i < powers.size; ++i) power_atEndOfRound(g, powers[i], target);
}
SPIRE_FN inline void at_end_of_turn(Game& g, int target) {
    Powers& powers = get_creature(g, target).powers;
    for (int i = 0; i < powers.size; ++i) {
        if (target != player_target) power_atEndOfTurnPreEndTurnCards(g, powers[i], target, false);
        power_atEndOfTurn(g, powers[i], target, target == player_target);
    }
}
}
