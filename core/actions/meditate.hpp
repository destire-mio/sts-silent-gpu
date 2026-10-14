#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Meditate : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(Meditate, bool selecting;)
    SPIRE_SNAPSHOT("MeditateAction.player", player)
    SPIRE_SNAPSHOT("MeditateAction.numberOfCards", scalar, effect.extra)
    SPIRE_SNAPSHOT("MeditateAction.optional", scalar, false)
    static SPIRE_FN void move(Game& g, int card, bool mark_full) {
        if (mark_full || g.hand.size < hand_limit)
            g.cards[card].retain = true;
        if (g.hand.size < hand_limit) {
            remove_from_pile(g.discard, card);
            g.hand.push_back(card);
            card_applyPowers(g, card);
        }
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.Meditate.selecting) {
            if (g.discard.empty() || e.extra <= 0)
                return;
            if (g.discard.size <= e.extra) {
                const Pile cards = g.discard;
                for (int i = 0; i < cards.size; ++i)
                    move(g, cards[i], true);
                return;
            }
            g.grid_source = PileId::Discard;
            g.selected_cards.clear();
            g.grid_num_cards = e.extra;
            g.selection_any_number = g.selection_can_pick_zero = false;
            g.screen = Screen::Grid;
            e.payload.state.Meditate.selecting = true;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i)
            move(g, g.selected_cards[i], false);
        g.selected_cards.clear();
        refresh_hand(g);
    }
};
}
