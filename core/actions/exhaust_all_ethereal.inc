#pragma once
#include "core/hooks.hpp"
#include "core/actions/exhaust_card.hpp"

namespace spire::units::actions {
struct ExhaustAllEthereal : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN void execute(Game& g, Effect &) {
        for (int i = 0; i < g.hand.size; ++i)
            if (g.cards[g.hand[i]].ethereal)
                add_top(g, ExhaustCard::make(g.hand[i], PileId::Hand));
    }
};
}
