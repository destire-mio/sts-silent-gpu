#pragma once
#include "../hooks.hpp"

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
        e.state.BetterDiscardPileToHand.numberOfCards = count;
        e.state.BetterDiscardPileToHand.optional = optional;
        return e;
    }
    SPIRE_FN static Effect make(int count, int cost) {
        Effect e = make(count);
        e.state.BetterDiscardPileToHand.newCost = cost;
        e.state.BetterDiscardPileToHand.setCost = true;
        return e;
    }
    SPIRE_FN static void move(Game& g, const Effect& e, int card) {
        if (g.hand.size < hand_limit) {
            g.hand.push_back(card);
            if (e.state.BetterDiscardPileToHand.setCost && g.cards[card].cost_for_turn >= 0)
                g.cards[card].cost_for_turn = max(0, e.state.BetterDiscardPileToHand.newCost);
            remove_from_pile(g.discard, card);
        }
        card_applyPowers(g, card);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.BetterDiscardPileToHand.selecting) {
            const int count = e.state.BetterDiscardPileToHand.numberOfCards;
            if (g.discard.empty() || count <= 0) return;
            if (g.discard.size <= count && !e.state.BetterDiscardPileToHand.optional) {
                const Pile cards = g.discard;
                for (int i = 0; i < cards.size; ++i) move(g, e, cards[i]);
                return;
            }
            g.grid_source = PileId::Discard;
            g.selected_cards.clear();
            g.selection_amount = count;
            g.selection_any_number = e.state.BetterDiscardPileToHand.optional;
            g.selection_can_pick_zero = e.state.BetterDiscardPileToHand.optional;
            e.state.BetterDiscardPileToHand.selecting = true;
            g.screen = Screen::Grid;
            return;
        }
        for (int i = 0; i < g.selected_cards.size; ++i) move(g, e, g.selected_cards[i]);
        g.selected_cards.clear();
        for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    }
};
}
