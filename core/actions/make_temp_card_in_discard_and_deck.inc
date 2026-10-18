#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct MakeTempCardInDiscardAndDeck : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("MakeTempCardInDiscardAndDeckAction.cardToMake", card, effect.card)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int draw = copy_card(g, e.card);
        upgrade_created_card(g, draw);
        insert_random_card(g, g.draw, draw);
        const int discard = copy_card(g, e.card);
        upgrade_created_card(g, discard);
        g.discard.push_back(discard);
    }
};
}
