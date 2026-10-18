#pragma once
#include "core/hooks.hpp"
#include "core/actions/setup.hpp"

namespace spire::units::cards {
struct Setup : CardHooks {
    SPIRE_SNAPSHOT("AbstractCard.upgradedCost", scalar, card.upgrades > 0)
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, actions::Setup::construct());
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
