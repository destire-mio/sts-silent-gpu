#pragma once
#include "core/hooks.hpp"
#include "core/actions/discovery.hpp"

namespace spire::units::cards {
struct Discovery : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, units::actions::Discovery::construct());
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.exhaust = false;
        }
    }
};
}
