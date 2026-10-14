#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Judgement : ActionHooks {
    SPIRE_WAITING_RULE("JudgementAction: waiting for a ported caller with three-seed oracle replay")
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
