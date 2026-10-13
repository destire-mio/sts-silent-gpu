#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MakeTempCardInHand : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("MakeTempCardInHandAction.c", card, effect.card)
    SPIRE_SNAPSHOT("MakeTempCardInHandAction.sameUUID", scalar, effect.state.MakeTempCardInHand.same_uuid)
    SPIRE_SNAPSHOT("MakeTempCardInHandAction.isOtherCardInCenter", scalar, !effect.state.MakeTempCardInHand.other_card_not_in_center)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(MakeTempCardInHand, bool same_uuid; bool other_card_not_in_center;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.amount == 0) return;
        const int hand_amount = min(e.amount, hand_limit - g.hand.size);
        for (int i = 0; i < hand_amount; ++i) {
            const int card = copy_card(g, e.card, e.state.MakeTempCardInHand.same_uuid);
            g.hand.push_back(card);
            card_triggerWhenCopied(g, card);
            refresh_hand(g);
            for (int j = 0; j < g.hand.size; ++j) card_applyPowers(g, g.hand[j]);
            on_draw_or_discard(g);
        }
        // ShowCardAndAddToDiscardEffect(AbstractCard,float,float) inserts srcCard.
        for (int i = hand_amount; i < e.amount; ++i)
            g.discard.push_back(copy_card(g, e.card, e.state.MakeTempCardInHand.same_uuid));
    }
};
}
