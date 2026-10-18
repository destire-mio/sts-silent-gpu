#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DiscardPileToTopOfDeck : ActionHooks {
    SPIRE_SNAPSHOT("DiscardPileToTopOfDeckAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(DiscardPileToTopOfDeck, bool selecting;)
    static SPIRE_FN void move(Game& g, int card) {
        remove_from_pile(g.discard, card);
        g.draw.push_back(card);
        mark_card_move(g, card);
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (monsters_basically_dead(g))
            return;
        if (!e.payload.state.DiscardPileToTopOfDeck.selecting) {
            if (g.discard.empty())
                return;
            if (g.discard.size == 1) {
                move(g, g.discard[0]);
                return;
            }
            open_grid_selection(g, PileId::Discard, 1, false);
            g.selection_can_pick_zero = false;
            e.payload.state.DiscardPileToTopOfDeck.selecting = true;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i)
            move(g, g.selected_cards[i]);
        g.selected_cards.clear();
        refresh_hand(g);
    }
};
}
