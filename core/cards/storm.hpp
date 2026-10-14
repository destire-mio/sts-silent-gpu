#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Storm : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, 1, 0, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Storm, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.innate = true;
        }
    }
};
}
