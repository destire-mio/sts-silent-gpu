#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AddCardDiscard : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.amount; ++i) {
            const int card = g.cards.size;
            g.cards.push_back(stat_equivalent_copy(g, e.card));
            g.cards[card].uuid = card + 1;
            g.discard.push_back(card);
        }
    }
};
}
