#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct BiasedCognition : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, 4, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Focus, g.cards[card].magic);
        queue_power(g, player_target, player_target, PowerId::Bias, 1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
