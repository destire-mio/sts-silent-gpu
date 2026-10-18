#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct RiddleWithHoles : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 3, -1, -1, 1, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        for (int i = 0; i < 5; ++i) queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
