#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::cards {
struct IronWave : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 5, 5, -1, 2, 2, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_block(g, player_target, g.cards[card].block);
        if (target != no_target) add_bottom(g, actions::VFX::make());
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
