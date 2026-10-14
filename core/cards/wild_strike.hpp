#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct WildStrike : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 1, 12, -1, -1, 5, 0, 0, false, false};
        d.strike = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::MakeTempCardInDrawPile;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Wound, 0));
        e.amount = 1;
        e.payload.state.MakeTempCardInDrawPile.random_spot = true;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 5;
        }
    }
};
}
