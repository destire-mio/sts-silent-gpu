#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DiscardToHand : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_SNAPSHOT("DiscardToHandAction.card", card, effect.card)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.hand.size < hand_limit) {
            for (int i = 0; i < g.discard.size; ++i) {
                if (g.discard[i] != e.card) continue;
                g.hand.push_back(e.card);
                card_applyPowers(g, e.card);
                g.discard.erase(i);
                break;
            }
        }
        refresh_hand(g);
    }
};
}
