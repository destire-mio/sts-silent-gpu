#pragma once
#include "../hooks.hpp"
#include "apply_power.hpp"

namespace spire::units::actions {
struct NightmareAction : ActionHooks {
    SPIRE_STATE(NightmareAction, bool selecting;)
    SPIRE_FN static void prepare(Game&, Effect& e) { action_initialize_state(e); }
    SPIRE_FN static void apply(Game& g, const Effect& e, int card) {
        Power power{PowerId::Nightmare, e.amount};
        power_constructPower(g, power, e.target, e.target, 0, card);
        add_top(g, ApplyPower::make(e.target, e.target, power, 1));
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.NightmareAction.selecting) {
            if (g.hand.empty()) { e.done = true; return; }
            if (g.hand.size == 1) {
                apply(g, e, g.hand[0]);
                e.done = true;
                return;
            }
            open_hand_selection(g, 1, false, false);
            e.state.NightmareAction.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            apply(g, e, g.selected_cards[0]);
            g.hand.push_back(g.selected_cards[0]);
            g.selected_cards.clear();
            g.selected_cards_retrieved = true;
        }
        e.done = true;
    }
};
}
