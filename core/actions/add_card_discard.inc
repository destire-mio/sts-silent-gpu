#pragma once
#include "core/hooks.hpp"

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
        e.extra = 1;
        return e;
    }
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        if (e.extra == 1 && !e.flag) upgrade_created_card(g, e.card);
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        // MakeTempCardInDiscardAction.update has no creation branch for six or more cards.
        if (e.amount >= 6) return;
        for (int i = 0; i < e.amount; ++i) {
            const int card = g.cards.add(e.flag ? card_makeSameInstanceOf(g, e.card) : card_makeStatEquivalentCopy(g, e.card));
            g.cards[card].uuid = e.flag ? g.cards[e.card].uuid : g.next_card_uuid++;
            upgrade_created_card(g, card);
            g.discard.push_back(card);
            mark_card_move(g, card);
        }
    }
};
}
