#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Loop : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Loop, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
