#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct LegSweep : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 2, -1, 11, 2, 0, 3, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_power(g, player_target, target, PowerId::Weak, g.cards[card].magic);
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_block += data().upgrade_block;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
