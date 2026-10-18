#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Judgement : ActionHooks {
    SPIRE_SNAPSHOT("JudgementAction.cutoff", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (get_creature(g, e.target).hp <= e.extra && e.target >= 0) {
            Effect a{};
            a.kind = EffectKind::InstantKill;
            a.target = e.target;
            a.source = no_target;
            add_top(g, a);
        }
    }
};
}
