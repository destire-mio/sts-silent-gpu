#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ApplyPowerToRandomEnemy : ActionHooks {
    SPIRE_FN static Effect construct(int source, const Power& power, int stack_amount = -1) {
        Effect e{};
        e.kind = EffectKind::ApplyPowerToRandomEnemy;
        action_initialize_state(e);
        e.source = source;
        e.target = no_target;
        e.amount = stack_amount;
        e.power = power.id;
        e.prepared_power = power;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.target = random_living_monster(g);
        if (e.target == no_target) return;
        Effect apply{};
        apply.kind = EffectKind::ApplyPower;
        apply.source = e.source;
        apply.target = e.target;
        apply.power = e.power;
        apply.amount = e.amount;
        apply.prepared_power = e.prepared_power;
        apply.prepared_power_supplied = true;
        add_top(g, apply);
    }
};
}
