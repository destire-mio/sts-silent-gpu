#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ReducePower : ActionHooks {
    SPIRE_FN static Effect construct(int target, int source, PowerId power, int amount) {
        Effect e{};
        e.kind = EffectKind::ReducePower;
        action_initialize_state(e);
        e.target = target;
        e.source = source;
        e.power = power;
        e.amount = amount;
        return e;
    }
    SPIRE_FN static Effect construct(int target, int source, const Power& power, int amount) {
        SPIRE_ASSERT(power.instance != 0);
        Effect e = construct(target, source, power.id, amount);
        e.power_instance = power.instance;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.power_instance == 0) { reduce_power(g, e.target, e.power, e.amount, e.source); return; }
        Power& power = power_instance(g, e.target, e.power_instance);
        if (e.amount < power.amount) {
            power_reducePower(g, power, e.target, e.amount);
            refresh_powers(g);
        } else {
            Effect remove{};
            remove.kind = EffectKind::RemovePower;
            remove.source = e.source;
            remove.target = e.target;
            remove.power = power.id;
            remove.power_instance = power.instance;
            add_top(g, remove);
        }
    }
};
}
