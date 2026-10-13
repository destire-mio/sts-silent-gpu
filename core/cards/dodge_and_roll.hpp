#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct DodgeAndRoll : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 4, -1, 0, 2, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        queue_power(g, player_target, player_target, PowerId::NextTurnBlock, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_block += data().upgrade_block;
        }
    }
};
}
