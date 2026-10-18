#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct EvokeOrb : ActionHooks {
    SPIRE_SNAPSHOT("EvokeOrbAction.orbCount", scalar, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN Effect make(int count) {
        Effect e{}; e.kind = EffectKind::EvokeOrb; e.source = e.target = no_target; e.extra = count; return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.extra; ++i) evoke_orb(g);
    }
};
}
