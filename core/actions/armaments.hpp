#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Armaments : ActionHooks {
    SPIRE_STATE(Armaments, uint16_t cannot_upgrade[card_capacity]; int cannot_upgrade_count; bool upgraded; bool selecting;)
    SPIRE_FN static void return_cards(Game& g, const Effect& e) {
        for (int i = 0; i < e.state.Armaments.cannot_upgrade_count; ++i)
            g.hand.push_back(e.state.Armaments.cannot_upgrade[i]);
        refresh_hand(g);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.Armaments.selecting) {
            if (e.state.Armaments.upgraded) {
                for (int i = 0; i < g.hand.size; ++i) {
                    const int card = g.hand[i];
                    if (!card_canUpgrade(g, card)) continue;
                    card_upgrade(g.cards[card]);
                    card_applyPowers(g, card);
                }
                return;
            }
            for (int i = 0; i < g.hand.size; ++i)
                if (!card_canUpgrade(g, g.hand[i]))
                    e.state.Armaments.cannot_upgrade[e.state.Armaments.cannot_upgrade_count++] = g.hand[i];
            const int count = g.hand.size - e.state.Armaments.cannot_upgrade_count;
            if (count == 0) return;
            if (count == 1) {
                for (int i = 0; i < g.hand.size; ++i) {
                    const int card = g.hand[i];
                    if (!card_canUpgrade(g, card)) continue;
                    card_upgrade(g.cards[card]);
                    card_applyPowers(g, card);
                    return;
                }
            }
            for (int i = 0; i < g.hand.size;)
                if (!card_canUpgrade(g, g.hand[i])) g.hand.erase(i);
                else ++i;
            open_hand_selection(g, 1, false, false, true);
            e.state.Armaments.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i) {
                const int card = g.selected_cards[i];
                card_upgrade(g.cards[card]);
                card_applyPowers(g, card);
                g.hand.push_back(card);
            }
            return_cards(g, e);
            g.selected_cards_retrieved = true;
            g.selected_cards.clear();
        }
    }
};
}
