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
SPIRE_FN inline void add_bottom(Game& g, const Effect& effect) { g.manager.actions.push_back(effect); }
SPIRE_FN inline void add_top(Game& g, const Effect& effect) { g.manager.actions.push_front(effect); }
SPIRE_FN inline int power_priority(PowerId id) {
    switch (id) {
        case PowerId::Weak: return 99;
        case PowerId::Frail: return 10;
        default: return 5;
    }
}
SPIRE_FN inline bool is_debuff(PowerId id, int amount) {
    switch (id) {
        case PowerId::Weak: case PowerId::Vulnerable: case PowerId::Frail:
        case PowerId::Entangled: case PowerId::NoDraw: return true;
        case PowerId::Strength: case PowerId::Dexterity: return amount <= 0;
        default: return false;
    }
}
SPIRE_FN inline int calculate_damage(const Game& g, int source, int target, int base, DamageType type = DamageType::Normal) {
    float value = static_cast<float>(base);
    if (type == DamageType::Normal) {
        if (source != no_target) {
            const Powers& powers = get_creature(g, source).powers;
            for (int i = 0; i < powers.size; ++i) {
                switch (powers[i].id) {
                    case PowerId::Strength: value += static_cast<float>(powers[i].amount); break;
                    case PowerId::Weak: value *= 0.75f; break;
                    default: break;
                }
            }
        }
        if (target != no_target) {
            const Powers& powers = get_creature(g, target).powers;
            for (int i = 0; i < powers.size; ++i) {
                if (powers[i].id == PowerId::Vulnerable) value *= 1.5f;
            }
        }
    }
    return max(0, floor_int(value));
}
SPIRE_FN inline int calculate_block(const Game& g, int base) {
    float value = static_cast<float>(base);
    const Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) {
        switch (powers[i].id) {
            case PowerId::Dexterity:
                value += static_cast<float>(powers[i].amount);
                if (value < 0.0f) value = 0.0f;
                break;
            case PowerId::Frail: value *= 0.75f; break;
            default: break;
        }
    }
    return max(0, floor_int(value));
}
SPIRE_FN inline void refresh_powers(Game& g) {
    for (int i = 0; i < g.hand.size; ++i) {
        Card& card = g.cards[g.hand[i]];
        card.damage = calculate_damage(g, player_target, no_target, card.base_damage);
        card.block = calculate_block(g, card.base_block);
    }
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int id = g.monster_order[i];
        Monster& m = g.monsters[id];
        if (m.pending_base_damage >= 0) m.intent_damage = calculate_damage(g, id, player_target, m.pending_base_damage);
    }
}
SPIRE_FN inline void queue_damage(Game& g, int source, int target, int base, DamageType type = DamageType::Normal, int steal = 0) {
    Effect e{};
    e.kind = EffectKind::Damage;
    e.source = source;
    e.target = target;
    e.amount = calculate_damage(g, source, target, base, type);
    e.damage_type = type;
    e.extra = steal;
    add_bottom(g, e);
}
SPIRE_FN inline void queue_block(Game& g, int target, int amount) {
    Effect e{};
    e.kind = EffectKind::Block;
    e.target = target;
    e.amount = amount;
    add_bottom(g, e);
}
SPIRE_FN inline void queue_power(Game& g, int source, int target, PowerId id, int amount, bool monster_source = false) {
    if (monsters_basically_dead(g)) return;
    Effect e{};
    e.kind = EffectKind::ApplyPower;
    e.source = source;
    e.target = target;
    e.power = id;
    e.amount = amount;
    // VulnerablePower.<init> differs from WeakPower/FrailPower before the enemy turn.
    e.flag = monster_source && (id != PowerId::Vulnerable || g.manager.turn_has_ended);
    add_bottom(g, e);
}
SPIRE_FN inline void gain_block(Game& g, int target, int amount) {
    Creature& c = get_creature(g, target);
    if (!c.dead && !c.dying && !c.escaped) c.block = min(999, c.block + amount);
    refresh_powers(g);
}
SPIRE_FN inline void remove_power(Game& g, int target, PowerId id) {
    Creature& c = get_creature(g, target);
    const int index = find_power(c, id);
    if (index >= 0) c.powers.erase(index);
    refresh_powers(g);
}
SPIRE_FN inline void apply_power(Game& g, int source, int target, PowerId id, int amount, bool just_applied = false) {
    (void)source;
    Creature& c = get_creature(g, target);
    if (!alive(c)) return;
    const int existing = find_power(c, id);
    if (id == PowerId::NoDraw && existing >= 0) return;
    if (is_debuff(id, amount) && has_power(c, PowerId::Artifact)) {
        Effect e{};
        e.kind = EffectKind::ReducePower;
        e.target = target;
        e.power = PowerId::Artifact;
        e.amount = 1;
        add_top(g, e);
        return;
    }
    if (existing >= 0) {
        Power& power = c.powers[existing];
        if (power.amount != -1 || id == PowerId::Strength || id == PowerId::Dexterity) power.amount += amount;
        if (id == PowerId::Strength || id == PowerId::Dexterity) {
            power.amount = max(-999, min(999, power.amount));
            if (power.amount == 0) {
                Effect e{};
                e.kind = EffectKind::RemovePower;
                e.target = target;
                e.power = id;
                add_top(g, e);
            }
        }
    } else {
        if (id == PowerId::Strength || id == PowerId::Dexterity) amount = max(-999, min(999, amount));
        Power power{};
        power.id = id;
        power.amount = amount;
        power.just_applied = just_applied;
        int index = c.powers.size;
        while (index > 0 && power_priority(c.powers[index - 1].id) > power_priority(id)) --index;
        c.powers.insert(index, power);
    }
    refresh_powers(g);
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
        c.powers[index].amount -= amount;
        refresh_powers(g);
    }
}
SPIRE_FN inline void at_end_of_round(Game& g, int target) {
    Powers& powers = get_creature(g, target).powers;
    for (int i = 0; i < powers.size; ++i) {
        Power& power = powers[i];
        switch (power.id) {
            case PowerId::Weak: case PowerId::Vulnerable: case PowerId::Frail: {
                if (power.just_applied) { power.just_applied = false; break; }
                Effect e{};
                e.kind = power.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
                e.target = target;
                e.power = power.id;
                e.amount = 1;
                add_bottom(g, e);
                break;
            }
            case PowerId::Ritual:
                if (target != player_target) {
                    if (power.skip_first) power.skip_first = false;
                    else queue_power(g, target, target, PowerId::Strength, power.amount);
                }
                break;
            default: break;
        }
    }
}
SPIRE_FN inline void at_end_of_turn(Game& g, int target) {
    const Powers& powers = get_creature(g, target).powers;
    for (int i = 0; i < powers.size; ++i) {
        const Power& p = powers[i];
        if (target == player_target && p.id == PowerId::Ritual) queue_power(g, target, target, PowerId::Strength, p.amount);
        if (target == player_target && (p.id == PowerId::Entangled || p.id == PowerId::NoDraw)) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.target = target;
            e.power = p.id;
            add_bottom(g, e);
        }
    }
}
}
