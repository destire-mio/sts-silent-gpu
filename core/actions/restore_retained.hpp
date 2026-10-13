#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RestoreRetained : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.retained.size;) {
            const int card = g.retained[i];
            if (!g.cards[card].retain && !g.cards[card].self_retain) {
                ++i;
                continue;
            }
            card_onRetained(g, card);
            g.hand.push_back(card);
            g.cards[card].retain = false;
            g.retained.erase(i);
        }
    }
};
}
