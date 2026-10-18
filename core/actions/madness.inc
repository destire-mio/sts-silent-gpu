#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Madness : ActionHooks {
    SPIRE_SNAPSHOT("MadnessAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect&) {
        bool better = false;
        bool possible = false;
        for (int i = 0; i < g.hand.size; ++i) {
            const Card& card = g.cards[g.hand[i]];
            if (card.cost_for_turn > 0) better = true;
            else if (card.cost > 0) possible = true;
        }
        if (!better && !possible) return;
        // MadnessAction.findAndModifyCard retries over the entire hand, consuming cardRandomRng each time.
        for (;;) {
            Card& card = g.cards[g.hand[g.rng.card_random.random(g.hand.size - 1)]];
            if (better ? card.cost_for_turn <= 0 : card.cost <= 0) continue;
            card.cost = card.cost_for_turn = 0;
            return;
        }
    }
};
}
