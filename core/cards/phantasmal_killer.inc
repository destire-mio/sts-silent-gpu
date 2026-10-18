#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct PhantasmalKiller : CardHooks {
    SPIRE_SNAPSHOT("AbstractCard.upgradedCost", scalar, card.upgrades > 0)
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        queue_power(g, player_target, player_target, PowerId::Phantasmal, 1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
