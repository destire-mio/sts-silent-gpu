#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ApplyPower : ActionHooks {
    SPIRE_STATE(ApplyPower, bool constructed;)
    SPIRE_FN static Effect make(int target, int source, const Power& power, int amount) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        action_initialize_state(e);
        e.source = source;
        e.target = target;
        e.power = power.id;
        e.prepared_power = power;
        e.amount = amount;
        e.state.ApplyPower.constructed = true;
        return e;
    }
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        Power& power = e.prepared_power;
        if (!e.state.ApplyPower.constructed) {
            power.id = e.power;
            power.amount = e.amount;
            power.just_applied = e.flag;
            power_constructPower(g, power, e.target, e.source, e.extra, e.card);
        }
        if (power.instance == 0) power.instance = g.next_power_instance++;
        if (monsters_basically_dead(g)) e.done = true;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        apply_prepared_power(g, e.source, e.target, e.prepared_power, e.amount);
    }
};
}
