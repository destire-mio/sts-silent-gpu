#pragma once
#include "../hooks.hpp"
#include "wait.hpp"

namespace spire::units::actions {
struct RetainCards : ActionHooks {
    SPIRE_STATE(RetainCards, bool selecting;)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.target = player_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.payload.state.RetainCards.selecting) {
            open_hand_selection(g, e.amount, false, true, false, true);
            add_bottom(g, Wait::make(16));
            e.payload.state.RetainCards.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i) {
                const int index = g.selected_cards[i];
                if (!g.cards[index].ethereal) g.cards[index].retain = true;
                g.hand.push_back(index);
            }
            g.selected_cards_retrieved = true;
            // RetainCardsAction.update has 29 ticks left after opening the selection.
            advance_escape(g, 28);
        }
    }
};
}
