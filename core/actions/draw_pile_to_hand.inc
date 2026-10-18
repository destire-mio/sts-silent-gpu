#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DrawPileToHand : ActionHooks {
    SPIRE_SNAPSHOT("DrawPileToHandAction.p", player)
    SPIRE_SNAPSHOT("DrawPileToHandAction.typeToCheck", card_type, static_cast<int>(effect.payload.state.DrawPileToHand.type_to_check))
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(DrawPileToHand, CardType type_to_check;)
    SPIRE_FN static Effect construct(int amount, CardType type) {
        Effect e{};
        e.kind = EffectKind::DrawPileToHand;
        action_initialize_state(e);
        e.amount = amount;
        e.payload.state.DrawPileToHand.type_to_check = type;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (g.draw.empty()) return;
        Pile candidates{};
        for (int i = 0; i < g.draw.size; ++i) {
            const int card = g.draw[i];
            if (card_def(g.cards[card].id).type != e.payload.state.DrawPileToHand.type_to_check) continue;
            if (candidates.empty()) candidates.push_back(card);
            else candidates.insert(g.rng.card_random.random(candidates.size - 1), card);
        }
        if (candidates.empty()) return;
        for (int i = 0; i < e.amount; ++i) {
            if (candidates.empty()) continue;
            shuffle(candidates.values, candidates.size, g.rng.shuffle);
            const int card = candidates.erase(0);
            if (g.hand.size == hand_limit) { draw_pile_to_discard(g, card); continue; }
            remove_from_pile(g.draw, card);
            g.hand.push_back(card);
            refresh_hand(g);
            for (int j = 0; j < g.hand.size; ++j) card_applyPowers(g, g.hand[j]);
        }
    }
};
}
