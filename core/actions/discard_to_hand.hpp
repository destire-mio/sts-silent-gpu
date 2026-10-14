#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DiscardToHand : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_SNAPSHOT("DiscardToHandAction.card", card, effect.card)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.hand.size < hand_limit && remove_from_pile(g.discard, e.card)) {
            g.hand.push_back(e.card);
            card_applyPowers(g, e.card);
        }
        refresh_hand(g);
    }
};
}
