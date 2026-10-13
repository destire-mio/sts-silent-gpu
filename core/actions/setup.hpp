#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Setup : ActionHooks {
    SPIRE_STATE(Setup, bool selecting;)
    SPIRE_FN static Effect construct() {
        Effect e{};
        e.kind = EffectKind::Setup;
        action_initialize_state(e);
        return e;
    }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.Setup.selecting) {
            if (g.hand.empty()) return;
            if (g.hand.size == 1) {
                const int card = g.hand[g.hand.size - 1];
                if (g.cards[card].cost > 0) g.cards[card].free_to_play_once = true;
                hand_to_deck(g, card);
                refresh_hand(g);
                return;
            }
            g.selection_amount = 1;
            g.discard_remaining = 1;
            g.selected_cards.clear();
            g.selected_cards_retrieved = false;
            e.state.Setup.selecting = true;
            g.screen = Screen::Discard;
            return;
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i) {
                const int card = g.selected_cards[i];
                if (g.cards[card].cost > 0) g.cards[card].free_to_play_once = true;
                hand_to_deck(g, card);
            }
            refresh_hand(g);
            g.selected_cards_retrieved = true;
        }
    }
};
}
