#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EvokeWithoutRemovingOrb : ActionHooks {
    SPIRE_SNAPSHOT("EvokeWithoutRemovingOrbAction.orbCount", scalar, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN Effect make(int count) {
        Effect e{}; e.kind = EffectKind::EvokeWithoutRemovingOrb; e.source = e.target = no_target; e.extra = count; return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.extra; ++i) evoke_without_losing_orb(g);
    }
};
}
