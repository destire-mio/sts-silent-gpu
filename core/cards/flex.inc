#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Flex : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 2, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Strength, g.cards[card].magic);
        queue_power(g, player_target, player_target, PowerId::LoseStrength, g.cards[card].magic);
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
