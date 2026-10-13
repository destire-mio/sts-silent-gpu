#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct PutOnDeck : ActionHooks {
    SPIRE_STATIC("numPlaced", game.put_on_deck_num)
    SPIRE_SNAPSHOT("PutOnDeckAction.p", player)
    SPIRE_SNAPSHOT("PutOnDeckAction.isRandom", scalar, effect.flag)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(PutOnDeck, bool selecting;)
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.PutOnDeck.selecting) {
            e.amount = min(e.amount, g.hand.size);
            if (e.flag) {
                for (int i = 0; i < e.amount; ++i)
                    hand_to_deck(g, g.hand[g.rng.card_random.random(g.hand.size - 1)]);
            } else {
                if (g.hand.size > e.amount) {
                    g.put_on_deck_num = e.amount;
                    open_hand_selection(g, e.amount);
                    e.state.PutOnDeck.selecting = true;
                    return;
                }
                // PutOnDeckAction.update compares i with the shrinking hand size.
                for (int i = 0; i < g.hand.size; ++i)
                    hand_to_deck(g, g.hand[g.rng.card_random.random(g.hand.size - 1)]);
            }
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i)
                hand_to_deck(g, g.selected_cards[i]);
            refresh_hand(g);
            g.selected_cards_retrieved = true;
        }
    }
};
}
