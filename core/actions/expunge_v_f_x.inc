#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ExpungeVFX : ActionHooks {
    SPIRE_SNAPSHOT("ExpungeVFXAction.m", creature, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (alive(get_creature(g, e.extra))) {
            Effect a{};
            a.source = a.target = no_target;
            a.kind = EffectKind::VFX;
            add_top(g, a);
            a.kind = EffectKind::SFX;
            add_top(g, a);
            add_top(g, a);
        }
    }
};
}
