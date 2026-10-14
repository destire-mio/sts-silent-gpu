#pragma once
#include "../hooks.hpp"
#include "gain_energy.hpp"

namespace spire::units::actions {
struct Recycle : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(Recycle, bool selecting;)
    SPIRE_SNAPSHOT("RecycleAction.p", player)
    static SPIRE_FN void recycle(Game& g, int card) {
        const int cost = g.cards[card].cost_for_turn;
        if (cost == -1 || cost > 0)
            add_top(g, units::actions::GainEnergy::make(cost == -1 ? g.player.energy : cost));
        exhaust_card(g, card);
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.Recycle.selecting) {
            if (g.hand.empty())
                return;
            if (g.hand.size == 1) {
                recycle(g, g.hand[0]);
                return;
            }
            open_hand_selection(g, 1);
            e.payload.state.Recycle.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i)
                recycle(g, g.selected_cards[i]);
            g.selected_cards_retrieved = true;
            g.selected_cards.clear();
        }
    }
};
}
