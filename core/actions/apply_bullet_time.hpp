#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ApplyBulletTime : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.hand.size; ++i) {
            Card& card = g.cards[g.hand[i]];
            if (card.cost_for_turn >= 0) card.cost_for_turn = 0;
        }
    }
};
}
