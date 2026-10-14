#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MakeTempCardInDrawPile : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("MakeTempCardInDrawPileAction.cardToMake", card, effect.card)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(MakeTempCardInDrawPile, bool random_spot; bool to_bottom;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.amount; ++i) {
            const int card = copy_card(g, e.card);
            upgrade_created_card(g, card);
            mark_card_move(g, card);
            if (e.payload.state.MakeTempCardInDrawPile.to_bottom) g.draw.insert(0, card);
            else if (e.payload.state.MakeTempCardInDrawPile.random_spot) insert_random_card(g, g.draw, card);
            else g.draw.push_back(card);
        }
    }
};
}
