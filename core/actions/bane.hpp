#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Bane : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.done = true;
        if (e.target == no_target) return;
        const Creature& target = get_creature(g, e.target);
        if (!has_power(target, "Poison") || target.hp <= 0) return;
        if (e.damage_type != DamageType::Thorns && get_creature(g, e.source).dying) return;
        creature_damage(g, e);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
