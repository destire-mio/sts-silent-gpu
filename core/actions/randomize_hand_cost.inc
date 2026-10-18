#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct RandomizeHandCost : ActionHooks {
    SPIRE_SNAPSHOT("RandomizeHandCostAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect&) {
        for (int i = 0; i < g.hand.size; ++i) {
            Card& card = g.cards[g.hand[i]];
            if (card.cost < 0) continue;
            const int cost = g.rng.card_random.random(3);
            if (card.cost != cost) card.cost = card.cost_for_turn = cost;
        }
    }
};
}
