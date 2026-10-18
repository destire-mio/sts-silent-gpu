#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ForTheEyes : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_SNAPSHOT("ForTheEyesAction.m", creature, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.extra != no_target && g.monsters[e.extra].intent_base_damage >= 0) {
            Effect a{};
            a.kind = EffectKind::ApplyPower;
            a.source = player_target;
            a.target = e.extra;
            a.power = PowerId::Weak;
            a.amount = e.amount;
            add_top(g, a);
        }
    }
};
}
