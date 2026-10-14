#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SkillFromDeckToHand : ActionHooks {
    SPIRE_SNAPSHOT("SkillFromDeckToHandAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(SkillFromDeckToHand, bool selecting;)
    SPIRE_FN static Effect construct(int amount) {
        Effect e{};
        e.kind = EffectKind::SkillFromDeckToHand;
        action_initialize_state(e);
        e.amount = amount;
        return e;
    }
    SPIRE_FN static void prepare(Game&, Effect& e) { action_initialize_state(e); }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.payload.state.SkillFromDeckToHand.selecting) {
            g.grid_cards.clear();
            for (int i = 0; i < g.draw.size; ++i) {
                const int card = g.draw[i];
                if (card_def(g.cards[card].id).type != CardType::Skill) continue;
                if (g.grid_cards.empty()) g.grid_cards.push_back(card);
                else g.grid_cards.insert(g.rng.card_random.random(g.grid_cards.size - 1), card);
            }
            if (g.grid_cards.empty()) return;
            if (g.grid_cards.size == 1) {
                const int card = g.grid_cards[0];
                if (g.hand.size == hand_limit) draw_pile_to_discard(g, card);
                else {
                    remove_from_pile(g.draw, card);
                    g.hand.push_back(card);
                    refresh_hand(g);
                    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
                }
                return;
            }
            g.grid_source = PileId::Grid;
            g.selection_any_number = false;
            g.selection_amount = e.amount;
            g.selected_cards.clear();
            e.payload.state.SkillFromDeckToHand.selecting = true;
            g.screen = Screen::Grid;
            return;
        }
        if (!g.selected_cards.empty()) {
            for (int i = 0; i < g.selected_cards.size; ++i) {
                const int card = g.selected_cards[i];
                if (g.hand.size == hand_limit) draw_pile_to_discard(g, card);
                else { remove_from_pile(g.draw, card); g.hand.push_back(card); }
                refresh_hand(g);
                for (int j = 0; j < g.hand.size; ++j) card_applyPowers(g, g.hand[j]);
            }
            g.selected_cards.clear();
            refresh_hand(g);
        }
    }
};
}
