#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Discard : ActionHooks {
    SPIRE_STATIC("numDiscarded", game.discard_action_num)
    SPIRE_SNAPSHOT("DiscardAction.p", player)
    SPIRE_SNAPSHOT("DiscardAction.endTurn", scalar, effect.flag)
    static SPIRE_FN constexpr ActionData data() { return {false, "DISCARD"}; }
    SPIRE_STATE(Discard, bool selecting; bool isRandom;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.payload.state.Discard.isRandom = e.extra != 0;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.payload.state.Discard.selecting) {
            if (!g.selected_cards_retrieved) {
                for (int i = 0; i < g.selected_cards.size; ++i) {
                    const int card = g.selected_cards[i];
                    move_to_discard(g, card);
                    card_triggerOnManualDiscard(g, card);
                    increment_discard(g, e.flag);
                }
                g.selected_cards_retrieved = true;
                advance_escape(g, 5);
            }
            return;
        }
        if (monsters_basically_dead(g)) { e.done = true; return; }
        if (g.hand.size <= e.amount) {
            e.amount = g.hand.size;
            while (!g.hand.empty()) discard_card(g, g.hand.size - 1, e.flag);
            for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
        } else if (e.payload.state.Discard.isRandom) {
            for (int i = 0; i < e.amount; ++i) {
                const int card = g.hand.erase(g.rng.card_random.random(g.hand.size - 1));
                move_to_discard(g, card);
                card_triggerOnManualDiscard(g, card);
                increment_discard(g, e.flag);
            }
        } else {
            if (e.amount < 0) open_hand_selection(g, 99, true, true);
            else {
                g.discard_action_num = e.amount;
                open_hand_selection(g, e.amount, false, false);
            }
            for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
            e.payload.state.Discard.selecting = true;
        }
        // DiscardAction.update ticks ACTION_DUR_XFAST after the discard, including an empty hand.
        if (!e.payload.state.Discard.selecting) advance_escape(g, 6);
    }
};
}
