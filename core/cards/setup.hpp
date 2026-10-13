#pragma once
#include "../hooks.hpp"
#include "../actions/setup.hpp"

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
            const int diff = card.cost_for_turn - card.cost;
            card.cost = 0;
            if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + diff;
            card.cost_for_turn = max(0, card.cost_for_turn);
        }
    }
};
}
