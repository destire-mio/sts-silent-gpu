#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Dash : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 10, 10, -1, 3, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_block(g, player_target, g.cards[card].block);
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
