#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RestoreRetained : ActionHooks {
    SPIRE_SNAPSHOT("RestoreRetainedCardsAction.group", group, 25, "UNSPECIFIED", &game.retained)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        e.source = no_target;
        e.target = player_target;
        e.amount = -1;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.done = true;
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
