#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct CarveReality : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 1, 6, -1, -1, 4, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Smite, 0));
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 4;
        }
    }
};
}
