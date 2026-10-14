#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EvokeAllOrbs : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN void execute(Game& g, Effect &) {
        for (int i = 0; i < g.orbs.slots.size; ++i) {
            Effect a{};
            a.kind = EffectKind::EvokeOrb;
            a.extra = 1;
            a.source = a.target = no_target;
            add_top(g, a);
        }
    }
};
}
