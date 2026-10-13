#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AnimateOrb : ActionHooks {
    SPIRE_SNAPSHOT("AnimateOrbAction.orbCount", scalar, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {false, nullptr}; }
    static SPIRE_FN Effect make(int count) {
        Effect e{}; e.kind = EffectKind::AnimateOrb; e.source = e.target = no_target; e.extra = count; return e;
    }
    static SPIRE_FN void execute(Game& , const Effect& ) {
        
    }
};
}
