#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AttackFromDeckToHand : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", player)
    SPIRE_SNAPSHOT("AbstractGameAction.target", player)
    SPIRE_SNAPSHOT("AttackFromDeckToHandAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(AttackFromDeckToHand, bool selecting;)
    SPIRE_FN static void move_card(Game& g, int card) {
        for (int i = 0; i < g.draw.size; ++i)
            if (g.draw[i] == card) { g.draw.erase(i); break; }
        if (g.hand.size == hand_limit) {
            remove_queued_card(g, card);
            g.discard.push_back(card);
            on_draw_or_discard(g);
        } else g.hand.push_back(card);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.payload.state.AttackFromDeckToHand.selecting) {
            g.grid_cards.clear();
            for (int i = 0; i < g.draw.size; ++i) {
                const int card = g.draw[i];
                if (card_data(g.cards[card].id).type == CardType::Attack)
                    insert_random_card(g, g.grid_cards, card);
            }
            if (g.grid_cards.empty()) return;
            if (g.grid_cards.size == 1) {
                const bool full = g.hand.size == hand_limit;
                move_card(g, g.grid_cards[0]);
                if (!full) {
                    refresh_hand(g);
                    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
                }
                return;
            }
            g.grid_source = PileId::Grid;
            g.selection_any_number = false;
            g.grid_num_cards = e.amount;
            g.selected_cards.clear();
            e.payload.state.AttackFromDeckToHand.selecting = true;
            g.screen = Screen::Grid;
            return;
        }
        if (!g.selected_cards.empty()) {
            for (int i = 0; i < g.selected_cards.size; ++i) {
                move_card(g, g.selected_cards[i]);
                refresh_hand(g);
                for (int j = 0; j < g.hand.size; ++j) card_applyPowers(g, g.hand[j]);
            }
            g.selected_cards.clear();
            refresh_hand(g);
        }
    }
};
}
