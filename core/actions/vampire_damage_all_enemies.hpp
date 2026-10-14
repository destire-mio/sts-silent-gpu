#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct VampireDamageAllEnemies : ActionHooks {
    SPIRE_WAITING_RULE("VampireDamageAllEnemiesAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_SNAPSHOT("VampireDamageAllEnemiesAction.damage", damage_array, game.multi_damage.get(effect.card), game.cards[effect.card].multi_damage_count, effect.card, true)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        DamageArray values = g.multi_damage.get(e.card);
        const int count = g.cards[e.card].multi_damage_count;
        int amounts[monster_capacity]{};
        for (int i = 0; i < count; ++i)
            amounts[i] = values.at(g.damage_exceptions, i);
        Powers &powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i)
            power_onDamageAllEnemies(g, powers.at(g.power_objects, i), player_target, amounts);
        values.assign(g.damage_exceptions, amounts, count);
        g.multi_damage.set(e.card, values);
        int healing = 0;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            const Creature &c = g.monsters[target].creature;
            if (c.dying || c.hp <= 0 || c.escaped)
                continue;
            Effect hit{};
            hit.source = e.source;
            hit.target = target;
            hit.amount = hit.info_base = amounts[i];
            hit.damage_type = e.damage_type;
            creature_damage(g, hit);
            if (c.last_damage_taken <= 0)
                continue;
            healing += c.last_damage_taken;
            for (int j = 0; j < c.last_damage_taken / 2 && j < 10; ++j) {
                Effect v{};
                v.kind = EffectKind::VFX;
                v.source = v.target = no_target;
                add_bottom(g, v);
            }
        }
        if (healing > 0) {
            Effect a{};
            a.kind = EffectKind::Heal;
            a.source = a.target = e.source;
            a.amount = healing;
            add_bottom(g, a);
        }
        if (monsters_basically_dead(g))
            clear_post_combat_actions(g);
    }
};
}
