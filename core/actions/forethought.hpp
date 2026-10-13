#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Forethought : ActionHooks {
    SPIRE_SNAPSHOT("ForethoughtAction.p", player)
    SPIRE_STATE(Forethought, bool chooseAny; bool selecting;)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.Forethought.chooseAny = e.flag;
    }
    SPIRE_FN static void move(Game& g, int card) {
        if (g.cards[card].cost > 0) g.cards[card].free_to_play_once = true;
        move_to_bottom_of_deck(g, card);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.payload.state.Forethought.selecting) {
            if (g.hand.empty()) return;
            if (g.hand.size == 1 && !e.payload.state.Forethought.chooseAny) {
                move(g, g.hand[g.hand.size - 1]);
                return;
            }
            open_hand_selection(g, e.payload.state.Forethought.chooseAny ? 99 : 1,
                e.payload.state.Forethought.chooseAny, e.payload.state.Forethought.chooseAny, false);
            e.payload.state.Forethought.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i) move(g, g.selected_cards[i]);
            g.selected_cards_retrieved = true;
        }
    }
};
}
