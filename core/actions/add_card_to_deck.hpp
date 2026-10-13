#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AddCardToDeck : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("AddCardToDeckAction.cardToObtain", card, effect.card)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        obtain_card(g, g.cards[e.card]);
    }
};
}
