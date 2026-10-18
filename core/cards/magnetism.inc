#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Magnetism : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 2, -1, -1, 1, 0, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Magnetism, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 1);
        }
    }
};
}
