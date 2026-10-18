#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct RecklessCharge : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 0, 7, -1, -1, 3, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::MakeTempCardInDrawPile;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Dazed, 0));
        e.amount = 1;
        e.payload.state.MakeTempCardInDrawPile.random_spot = true;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
        }
    }
};
}
