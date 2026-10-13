#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RemovePower : ActionHooks {
    SPIRE_FN static Effect from_instance(int target, int source, const Power& power) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        action_initialize_state(e);
        e.target = target;
        e.source = source;
        e.flag = true;
        e.prepared_power = power;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (!e.flag) { remove_power(g, e.target, e.power, e.power_offset); return; }
        Creature& c = get_creature(g, e.target);
        if (c.dying || c.half_dead || (e.target != player_target && c.escaped)) return;
        for (int i = 0; i < c.powers.size; ++i) {
            if (c.powers[i].instance != e.prepared_power.instance) continue;
            remove_power_at(g, e.target, i);
            return;
        }
    }
};
}
