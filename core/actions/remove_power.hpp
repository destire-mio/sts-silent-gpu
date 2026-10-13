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
        e.power = power.id;
        e.power_instance = power.instance;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) { remove_power(g, e.target, e.power, e.power_instance); }
};
}
