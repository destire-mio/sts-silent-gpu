#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DamageAllEnemies : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.damageType", damage_type, effect.damage_type)
    SPIRE_SNAPSHOT("DamageAllEnemiesAction.damage", damage_array, effect.payload.state.DamageAllEnemies.damage, effect.payload.state.DamageAllEnemies.damageCount, effect.card, !(effect.payload.state.DamageAllEnemies.utilizeBaseDamage && effect.payload.state.DamageAllEnemies.firstFrame))
    SPIRE_STATE(DamageAllEnemies, DamageArray damage; int baseDamage; uint8_t damageCount; bool firstFrame; bool utilizeBaseDamage;)
    SPIRE_FN static constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static Effect make(int base_damage, DamageType type = DamageType::Normal) {
        Effect e{};
        e.kind = EffectKind::DamageAllEnemies;
        action_initialize_state(e);
        e.target = no_target;
        e.damage_type = type;
        e.payload.state.DamageAllEnemies.firstFrame = true;
        e.payload.state.DamageAllEnemies.baseDamage = base_damage;
        e.payload.state.DamageAllEnemies.utilizeBaseDamage = true;
        return e;
    }
    SPIRE_FN static Effect make(Game& g, int source, const int* amounts, int count, DamageType type = DamageType::Normal) {
        Effect e = make(0, type);
        e.source = source;
        e.payload.state.DamageAllEnemies.utilizeBaseDamage = false;
        e.payload.state.DamageAllEnemies.damageCount = count;
        SPIRE_ASSERT(count <= monster_capacity);
        e.payload.state.DamageAllEnemies.damage.assign(g.damage_exceptions, amounts, count);
        return e;
    }
    SPIRE_FN static Effect from_card(const Game& g, int card, DamageType type = DamageType::Normal) {
        Effect e = make(0, type);
        e.payload.state.DamageAllEnemies.utilizeBaseDamage = false;
        e.card = card;
        e.payload.state.DamageAllEnemies.damageCount = g.cards[card].multi_damage_count;
        return e;
    }
    SPIRE_FN static void release(Game& g, Effect& e) {
        if (e.card < 0) g.damage_exceptions.release(e.payload.state.DamageAllEnemies.damage.head);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        auto& state = e.payload.state.DamageAllEnemies;
        if (state.firstFrame) {
            if (state.utilizeBaseDamage) {
                state.damageCount = g.monster_order.size;
                int amounts[monster_capacity]{};
                for (int i = 0; i < g.monster_order.size; ++i)
                    amounts[i] = calculate_damage(g, player_target, g.monster_order[i], state.baseDamage);
                state.damage.assign(g.damage_exceptions, amounts, state.damageCount);
            }
            state.firstFrame = false;
        }
        DamageArray damage = e.card >= 0 ? g.multi_damage.get(e.card) : state.damage;
        int amounts[monster_capacity]{};
        for (int i = 0; i < state.damageCount; ++i) amounts[i] = damage.at(g.damage_exceptions, i);
        Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) power_onDamageAllEnemies(g, powers.at(g.power_objects, i), player_target, amounts);
        damage.assign(g.damage_exceptions, amounts, state.damageCount);
        if (e.card >= 0) g.multi_damage.set(e.card, damage);
        else state.damage = damage;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            const Creature& monster = g.monsters[owner].creature;
            if (monster.dying || monster.half_dead || monster.escaped) continue;
            Effect info{};
            info.source = e.source;
            info.target = owner;
            info.info_base = info.amount = amounts[i];
            info.damage_type = e.damage_type;
            monster_damage(g, owner, info);
        }
        if (monsters_basically_dead(g)) {
            clear_post_combat_actions(g);
        }
    }
};
}
