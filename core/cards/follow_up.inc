#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct FollowUp : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::FollowUp;
        e.source = e.target = no_target;
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
