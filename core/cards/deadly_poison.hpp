#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct DeadlyPoison : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 1, -1, -1, 5, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_power(g, player_target, target, PowerId::Poison, g.cards[card].magic);
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
