#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ExhaustAllEthereal : ActionHooks {
    SPIRE_WAITING_RULE("ExhaustAllEtherealAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN void execute(Game& g, Effect &) {
        for (int i = 0; i < g.hand.size; ++i)
            if (g.cards[g.hand[i]].ethereal) {
                Effect a{};
                a.kind = EffectKind::ExhaustCard;
                action_initialize_state(a);
                a.source = a.target = no_target;
                a.card = g.hand[i];
                add_top(g, a);
            }
    }
};
}
