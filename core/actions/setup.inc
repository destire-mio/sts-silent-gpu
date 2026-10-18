#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Setup : ActionHooks {
    SPIRE_SNAPSHOT("SetupAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
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
        if (!e.payload.state.Setup.selecting) {
            if (g.hand.empty()) return;
            if (g.hand.size == 1) {
                const int card = g.hand[g.hand.size - 1];
                if (g.cards[card].cost > 0) g.cards[card].free_to_play_once = true;
                hand_to_deck(g, card);
                refresh_hand(g);
                return;
            }
            open_hand_selection(g, 1);
            e.payload.state.Setup.selecting = true;
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
