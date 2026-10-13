#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AddCardDiscard : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("MakeTempCardInDiscardAction.c", card, effect.card)
    SPIRE_SNAPSHOT("MakeTempCardInDiscardAction.numCards", scalar, effect.amount)
    SPIRE_SNAPSHOT("MakeTempCardInDiscardAction.sameUUID", scalar, effect.flag)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static Effect make(int card, int amount) {
        Effect e{};
        e.kind = EffectKind::AddCardDiscard;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = card;
        e.amount = amount;
        return e;
    }
    SPIRE_FN static Effect make(int card, bool same_uuid) {
        Effect e = make(card, 1);
        e.flag = same_uuid;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        // MakeTempCardInDiscardAction.update has no creation branch for six or more cards.
        if (e.amount >= 6) return;
        for (int i = 0; i < e.amount; ++i) {
            const int card = g.cards.size;
            g.cards.push_back(e.flag ? card_makeSameInstanceOf(g, e.card) : card_makeStatEquivalentCopy(g, e.card));
            g.cards[card].uuid = e.flag ? g.cards[e.card].uuid : card + 1;
            g.discard.push_back(card);
        }
    }
};
}
