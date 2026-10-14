#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct BetterDrawPileToHand : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(BetterDrawPileToHand, int numberOfCards; bool optional; bool selecting;)
    SPIRE_SNAPSHOT("BetterDrawPileToHandAction.player", player)
    static SPIRE_FN void move(Game& g, int card) {
        if (g.hand.size == hand_limit)
            draw_pile_to_discard(g, card);
        else {
            remove_from_pile(g.draw, card);
            g.hand.push_back(card);
            card_applyPowers(g, card);
            on_draw_or_discard(g);
        }
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        auto &state = e.payload.state.BetterDrawPileToHand;
        if (!state.selecting) {
            if (g.draw.empty() || state.numberOfCards <= 0)
                return;
            if (g.draw.size <= state.numberOfCards && !state.optional) {
                const Pile cards = g.draw;
                for (int i = 0; i < cards.size; ++i)
                    move(g, cards[i]);
                return;
            }
            g.grid_cards = g.draw;
            sort_selection_cards(g);
            g.grid_source = PileId::Grid;
            g.selected_cards.clear();
            g.selection_amount = state.numberOfCards;
            g.selection_any_number = g.selection_can_pick_zero = state.optional;
            g.grid_previous_deck_size = g.grid_cards.size;
            g.screen = Screen::Grid;
            state.selecting = true;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i)
            move(g, g.selected_cards[i]);
        g.selected_cards.clear();
        refresh_hand(g);
    }
};
}
