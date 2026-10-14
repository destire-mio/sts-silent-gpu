#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"
namespace spire::units::cards {
struct SeeingRed : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, actions::GainEnergy::make(2));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.cost = card.cost_for_turn = 0;
            card.upgraded_cost = true;
        }
    }
};
}
