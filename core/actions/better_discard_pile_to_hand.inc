#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct BetterDiscardPileToHand : ActionHooks {
    SPIRE_SNAPSHOT("BetterDiscardPileToHandAction.player", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(BetterDiscardPileToHand, int numberOfCards; int newCost; bool optional; bool setCost; bool selecting;)
    SPIRE_FN static Effect make(int count, bool optional = false) {
        Effect e{};
        e.kind = EffectKind::BetterDiscardPileToHand;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.BetterDiscardPileToHand.numberOfCards = count;
        e.payload.state.BetterDiscardPileToHand.optional = optional;
        return e;
    }
    SPIRE_FN static Effect make(int count, int cost) {
        Effect e = make(count);
        e.payload.state.BetterDiscardPileToHand.newCost = cost;
        e.payload.state.BetterDiscardPileToHand.setCost = true;
        return e;
    }
    SPIRE_FN static void move(Game& g, const Effect& e, int card) {
        if (g.hand.size < hand_limit) {
            g.hand.push_back(card);
            if (e.payload.state.BetterDiscardPileToHand.setCost && g.cards[card].cost_for_turn >= 0)
                g.cards[card].cost_for_turn = max(0, e.payload.state.BetterDiscardPileToHand.newCost);
            remove_from_pile(g.discard, card);
        }
        card_applyPowers(g, card);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.payload.state.BetterDiscardPileToHand.selecting) {
            const int count = e.payload.state.BetterDiscardPileToHand.numberOfCards;
            if (g.discard.empty() || count <= 0) return;
            if (g.discard.size <= count && !e.payload.state.BetterDiscardPileToHand.optional) {
                const Pile cards = g.discard;
                for (int i = 0; i < cards.size; ++i) move(g, e, cards[i]);
                return;
            }
            // BetterDiscardPileToHandAction.update opens through GridCardSelectScreen.callOnOpen.
            open_grid_selection(g, PileId::Discard, count, false);
            g.grid_can_cancel = true;
            g.selection_any_number = e.payload.state.BetterDiscardPileToHand.optional;
            g.selection_can_pick_zero = e.payload.state.BetterDiscardPileToHand.optional;
            e.payload.state.BetterDiscardPileToHand.selecting = true;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i) move(g, e, g.selected_cards[i]);
        g.selected_cards.clear();
        for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    }
};
}
