#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Smite : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 1, 12, -1, -1, 4, 0, 0, true, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
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
