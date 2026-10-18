#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Impervious : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, 30, -1, 0, 10, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) ++card.upgrades;
    }
};
}
