#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct AutoShields : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 11, -1, 0, 4, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        if (g.player.creature.block == 0)
            queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;

        }
    }
};
}
