#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Discard : ActionHooks {
    SPIRE_STATE(Discard, bool selecting;)
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.state.Discard.selecting) {
            for (int i = 0; i < g.selected_cards.size; ++i) {
                const int card = g.selected_cards[i];
                g.hand.push_back(card);
                discard_card(g, g.hand.size - 1, false);
            }
            g.selected_cards_retrieved = true;
            return;
        }
        if (monsters_basically_dead(g)) { e.done = true; return; }
        if (g.hand.size <= e.amount) {
            e.amount = g.hand.size;
            while (!g.hand.empty()) discard_card(g, g.hand.size - 1, e.flag);
        } else {
            SPIRE_ASSERT(!e.flag);
            g.discard_action_num = e.amount;
            open_hand_selection(g, e.amount);
            e.state.Discard.selecting = true;
        }
    }
};
}
