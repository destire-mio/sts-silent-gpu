#pragma once
#include "core/hooks.hpp"
#include "core/actions/new_queue_card.hpp"

namespace spire::units::actions {
struct Omniscience : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(Omniscience, bool selecting;)
    SPIRE_SNAPSHOT("OmniscienceAction.player", player)
    SPIRE_SNAPSHOT("OmniscienceAction.playAmt", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.Omniscience.selecting) {
            if (g.draw.empty())
                return;
            g.grid_cards = g.draw;
            sort_selection_cards(g);
            open_grid_selection(g, PileId::Grid, 1, false);
            g.grid_can_cancel = true;
            g.selection_any_number = g.selection_can_pick_zero = false;
            e.payload.state.Omniscience.selecting = true;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i) {
            const int card = g.selected_cards[i];
            g.cards[card].exhaust = true;
            remove_from_pile(g.draw, card);
            add_bottom(g, units::actions::NewQueueCard::construct_random(card, true, false, true));
            for (int j = 0; j < e.extra - 1; ++j) {
                Card copy = card_makeStatEquivalentCopy(g, card);
                copy.purge_on_use = true;
                add_bottom(g, units::actions::NewQueueCard::construct_random(create_card(g, copy), true, false, true));
            }
        }
        g.selected_cards.clear();
        refresh_hand(g);
    }
};
}
