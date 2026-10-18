#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Disarm : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 1, -1, -1, 2, 0, 0, 1, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_power(g, player_target, target, PowerId::Strength, -g.cards[card].magic);
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
