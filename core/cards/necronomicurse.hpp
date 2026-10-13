#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Necronomicurse : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef data{CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
        data.purgeable = false;
        return data;
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void onRemoveFromMasterDeck(Game& g, Card&) {
        // NecronomicurseEffect constructor adds the replacement before its animation.
        Card card = make_card(CardId::Necronomicurse);
        card.uuid = g.next_card_uuid++;
        g.run.master_deck.push_back(card);
    }
    static SPIRE_FN void triggerOnExhaust(Game& g, int) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        e.card = create_card(g, make_card(CardId::Necronomicurse));
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card&) {}
};
}
