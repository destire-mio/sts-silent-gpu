#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct TalkToTheHand : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 5, -1, 2, 2, 0, 1, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        queue_power(g, player_target, target, PowerId::BlockReturn, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
