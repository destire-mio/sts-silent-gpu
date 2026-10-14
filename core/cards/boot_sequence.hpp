#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct BootSequence : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, 10, -1, 0, 3, 0, true, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;

        }
    }
};
}
