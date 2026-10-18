#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Streamline : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 15, -1, 1, 5, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::ReduceCost;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.amount = g.cards[card].magic;
        e.payload.state.ReduceCost.uuid = g.cards[card].uuid;
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
