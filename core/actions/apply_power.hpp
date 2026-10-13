#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ApplyPower : ActionHooks {
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        if (e.prepared_power_supplied) return;
        Power& power = e.prepared_power;
        power.id = e.power;
        power.amount = e.amount;
        power.just_applied = e.flag;
        power_initialize_state(power);
        power_initialPower(g, power, e.target);
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        apply_prepared_power(g, e.source, e.target, e.prepared_power, e.amount);
    }
};
}
