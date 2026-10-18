#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Melter : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 10, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::RemoveAllBlock;
        e.source = player_target;
        e.target = target;
        add_bottom(g, e);
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 4;
        }
    }
};
}
