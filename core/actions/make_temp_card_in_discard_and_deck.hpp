#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MakeTempCardInDiscardAndDeck : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int draw = copy_card(g, e.card);
        insert_random_card(g, g.draw, draw);
        g.discard.push_back(copy_card(g, e.card));
    }
};
}
