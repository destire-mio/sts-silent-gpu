#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MakeTempCardInDrawPile : ActionHooks {
    SPIRE_STATE(MakeTempCardInDrawPile, bool random_spot; bool to_bottom;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.amount; ++i) {
            const int card = copy_card(g, e.card);
            if (e.state.MakeTempCardInDrawPile.to_bottom) g.draw.insert(0, card);
            else if (e.state.MakeTempCardInDrawPile.random_spot) insert_random_card(g, g.draw, card);
            else g.draw.push_back(card);
        }
    }
};
}
