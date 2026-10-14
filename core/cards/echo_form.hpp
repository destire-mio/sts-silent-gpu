#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct EchoForm : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 3, -1, -1, -1, 0, 0, 0, false, true};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        queue_power(g, player_target, player_target, PowerId::Echo, 1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.ethereal = false;
        }
    }
};
}
