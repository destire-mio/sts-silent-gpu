#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Apparition : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, true, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        queue_power(g, player_target, player_target, PowerId::IntangiblePlayer, 1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.ethereal = false;
        }
    }
};
}
