#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct RemovePower : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("RemoveSpecificPowerAction.powerToRemove", power_id, effect.power, effect.extra)
    SPIRE_SNAPSHOT("RemoveSpecificPowerAction.powerInstance", power_reference, effect.target, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {false, "DEBUFF"}; }
    SPIRE_FN static Effect from_instance(int target, int source, const Power& power) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        action_initialize_state(e);
        e.target = target;
        e.source = source;
        e.power = power.id;
        e.extra = power.instance;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        // RemoveSpecificPowerAction.update refreshes before its 0.1s wait; a missing power ends immediately.
        const bool removed = remove_power(g, e.target, e.power, e.extra);
        settle_deaths_after_wait(g, removed ? 7 : 1);
    }
};
}
