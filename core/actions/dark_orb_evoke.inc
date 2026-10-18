#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DarkOrbEvoke : ActionHooks {
    SPIRE_SNAPSHOT("DarkOrbEvokeAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("DarkOrbEvokeAction.muteSfx", scalar, false)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN void prepare(Game& g, Effect& e) {
        e.target = no_target;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int m = g.monster_order[i]; const Creature& c = g.monsters[m].creature;
            if (!alive(c)) continue;
            if (e.target == no_target || c.hp < g.monsters[e.target].creature.hp) e.target = m;
        }
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target == no_target) return;
        e.amount = has_power(g, get_creature(g, e.target), PowerId::LockOn) ? static_cast<int>(e.info_base * 1.5f) : e.info_base;
        damage(g, e);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
