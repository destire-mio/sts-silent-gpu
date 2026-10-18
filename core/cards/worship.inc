#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Worship : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, -1, 5, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Mantra, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.self_retain = true;
        }
    }
};
}
