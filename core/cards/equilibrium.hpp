#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Equilibrium : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, 13, 1, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        queue_power(g, player_target, player_target, PowerId::Equilibrium, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
