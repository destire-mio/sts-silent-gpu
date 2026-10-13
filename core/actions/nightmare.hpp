#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct NightmareAction : ActionHooks {
    SPIRE_STATE(NightmareAction, bool selecting;)
    SPIRE_FN static void prepare(Game&, Effect& e) { action_initialize_state(e); }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.NightmareAction.selecting) {
            if (g.hand.size <= 1) { e.done = true; return; }
            g.selection_amount = 1;
            g.discard_remaining = 1;
            g.selection_any_number = false;
            g.selection_can_pick_zero = false;
            g.selected_cards.clear();
            g.selected_cards_retrieved = false;
            g.screen = Screen::Discard;
            e.state.NightmareAction.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            g.hand.push_back(g.selected_cards[0]);
            g.selected_cards.clear();
            g.selected_cards_retrieved = true;
        }
        e.done = true;
    }
};
}
