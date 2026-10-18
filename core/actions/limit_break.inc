#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct LimitBreak : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_SNAPSHOT("LimitBreakAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect &) {
        const int amount = power_amount(g, g.player.creature, PowerId::Strength);
        if (has_power(g, g.player.creature, PowerId::Strength)) {
            Effect a{};
            a.kind = EffectKind::ApplyPower;
            a.power = PowerId::Strength;
            a.amount = amount;
            add_top(g, a);
        }
    }
};
}
