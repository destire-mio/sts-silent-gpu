#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DamageAllEnemies : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.damageType", damage_type, effect.damage_type)
    SPIRE_SNAPSHOT("DamageAllEnemiesAction.damage", damage_array, effect.state.DamageAllEnemies.damage, effect.state.DamageAllEnemies.damageCount, effect.card, !(effect.state.DamageAllEnemies.utilizeBaseDamage && effect.state.DamageAllEnemies.firstFrame))
    SPIRE_STATE(DamageAllEnemies, int damage[monster_capacity]; int damageCount; int baseDamage; bool firstFrame; bool utilizeBaseDamage;)
    SPIRE_FN static constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static Effect make(int source, const int* amounts, int count, DamageType type = DamageType::Normal) {
        Effect e{};
        e.kind = EffectKind::DamageAllEnemies;
        action_initialize_state(e);
        e.source = source;
        e.target = no_target;
        e.damage_type = type;
        e.state.DamageAllEnemies.firstFrame = true;
        e.state.DamageAllEnemies.damageCount = count;
        SPIRE_ASSERT(count <= monster_capacity);
        for (int i = 0; i < count; ++i) e.state.DamageAllEnemies.damage[i] = amounts[i];
        return e;
    }
    SPIRE_FN static Effect make(int base_damage, DamageType type = DamageType::Normal) {
        Effect e = make(player_target, nullptr, 0, type);
        e.state.DamageAllEnemies.baseDamage = base_damage;
        e.state.DamageAllEnemies.utilizeBaseDamage = true;
        return e;
    }
    SPIRE_FN static Effect from_card(const Game& g, int card, DamageType type = DamageType::Normal) {
        Effect e = make(player_target, g.multi_damage[card], g.cards[card].multi_damage_count, type);
        e.card = card;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        auto& state = e.state.DamageAllEnemies;
        if (state.firstFrame) {
            if (state.utilizeBaseDamage) {
                state.damageCount = g.monster_order.size;
                for (int i = 0; i < g.monster_order.size; ++i)
                    state.damage[i] = calculate_damage(g, player_target, g.monster_order[i], state.baseDamage);
            }
            state.firstFrame = false;
        }
        int* amounts = e.card >= 0 ? g.multi_damage[e.card] : state.damage;
        Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) power_onDamageAllEnemies(g, powers.at(g.power_objects, i), player_target, amounts);
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
            g.battle_over = true;
            clear_post_combat_actions(g);
        }
    }
};
}
