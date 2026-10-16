#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Exhume : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(Exhume, bool selecting;)
    SPIRE_SNAPSHOT("ExhumeAction.p", player)
    SPIRE_SNAPSHOT("ExhumeAction.upgrade", scalar, effect.flag)
    SPIRE_SNAPSHOT("ExhumeAction.exhumes", card_array, game.selection_excluded.values, effect.payload.state.Exhume.selecting ? game.selection_excluded.size : 0)
    static SPIRE_FN void move(Game& g, const Effect& e, int card) {
        g.hand.push_back(card);
        Card& c = g.cards[card];
        if (has_power(g, g.player.creature, PowerId::Corruption) && card_data(c.id).type == CardType::Skill &&
            c.cost_for_turn >= 0)
            c.cost_for_turn = 0;
        remove_from_pile(g.exhaust, card);
        if (e.flag && card_canUpgrade(g, card))
            card_upgrade(c);
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.Exhume.selecting) {
            if (g.hand.size == hand_limit || g.exhaust.empty())
                return;
            if (g.exhaust.size == 1) {
                if (g.cards[g.exhaust[0]].id != CardId::Exhume)
                    move(g, e, g.exhaust[0]);
                return;
            }
            g.selection_excluded.clear();
            for (int i = 0; i < g.exhaust.size;) {
                if (g.cards[g.exhaust[i]].id == CardId::Exhume) {
                    g.selection_excluded.push_back(g.exhaust[i]);
                    g.exhaust.erase(i);
                } else
                    ++i;
            }
            if (g.exhaust.empty()) {
                g.exhaust = g.selection_excluded;
                g.selection_excluded.clear();
                return;
            }
            open_grid_selection(g, PileId::Exhaust, 1, false);
            g.grid_can_cancel = true;
            g.selection_can_pick_zero = false;
            e.payload.state.Exhume.selecting = true;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i)
            move(g, e, g.selected_cards[i]);
        g.selected_cards.clear();
        refresh_hand(g);
        for (int i = 0; i < g.selection_excluded.size; ++i)
            g.exhaust.push_back(g.selection_excluded[i]);
        g.selection_excluded.clear();
    }
};
}
