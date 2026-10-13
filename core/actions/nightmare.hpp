#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct NightmareAction : ActionHooks {
    SPIRE_STATE(NightmareAction, bool selecting;)
    SPIRE_FN static void prepare(Game&, Effect& e) { action_initialize_state(e); }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.NightmareAction.selecting) {
            if (g.hand.size <= 1) { e.done = true; return; }
            open_hand_selection(g, 1, false, false);
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
