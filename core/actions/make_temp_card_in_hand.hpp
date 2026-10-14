#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MakeTempCardInHand : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("MakeTempCardInHandAction.c", card, effect.card)
    SPIRE_SNAPSHOT("MakeTempCardInHandAction.sameUUID", scalar, effect.payload.state.MakeTempCardInHand.same_uuid)
    SPIRE_SNAPSHOT("MakeTempCardInHandAction.isOtherCardInCenter", scalar, !effect.payload.state.MakeTempCardInHand.other_card_not_in_center)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(MakeTempCardInHand, bool same_uuid; bool other_card_not_in_center;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.amount == 0) return;
        const int hand_amount = min(e.amount, hand_limit - g.hand.size);
        for (int i = 0; i < hand_amount; ++i) {
            const int card = copy_card(g, e.card, e.payload.state.MakeTempCardInHand.same_uuid);
            add_created_card_to_hand(g, card);
        }
        // ShowCardAndAddToDiscardEffect(AbstractCard,float,float) inserts srcCard.
        for (int i = hand_amount; i < e.amount; ++i) {
            const int card = copy_card(g, e.card, e.payload.state.MakeTempCardInHand.same_uuid);
            upgrade_created_card(g, card); g.discard.push_back(card);
        }
    }
};
}
