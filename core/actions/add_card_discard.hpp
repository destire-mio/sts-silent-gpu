#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AddCardDiscard : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.amount; ++i) {
            const int card = g.cards.size;
            g.cards.push_back(make_card(static_cast<CardId>(e.card)));
            g.discard.push_back(card);
        }
    }
};
}
