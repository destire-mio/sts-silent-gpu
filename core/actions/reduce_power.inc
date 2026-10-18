#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ReducePower : ActionHooks {
    SPIRE_SNAPSHOT("ReducePowerAction.powerID", power_id, effect.power, effect.extra)
    SPIRE_SNAPSHOT("ReducePowerAction.powerInstance", power_reference, effect.target, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {false, "REDUCE_POWER"}; }
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
        e.extra = power.instance;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.extra == 0) { reduce_power(g, e.target, e.power, e.amount, e.source); return; }
        Power& power = power_instance(g, e.extra);
        if (e.amount < power.amount) {
            power_reducePower(g, power, e.target, e.amount);
            refresh_powers(g);
        } else {
            Effect remove{};
            remove.kind = EffectKind::RemovePower;
            remove.source = e.source;
            remove.target = e.target;
            remove.power = power.id;
            remove.extra = power.instance;
            add_top(g, remove);
        }
    }
};
}
