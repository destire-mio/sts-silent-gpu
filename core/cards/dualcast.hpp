#pragma once
#include "../hooks.hpp"
#include "../actions/channel.hpp"
#include "../actions/animate_orb.hpp"
#include "../actions/evoke_orb.hpp"
#include "../actions/evoke_without_removing_orb.hpp"
#include "../actions/increase_max_orb.hpp"
#include "../actions/decrease_max_orb.hpp"

namespace spire::units::cards {
struct Dualcast : CardHooks {
    static SPIRE_FN constexpr CardDef data() { return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, false, false}; }
    static SPIRE_FN void use(Game& g, int , int ) {
        add_bottom(g, units::actions::AnimateOrb::make(1));
        add_bottom(g, units::actions::EvokeWithoutRemovingOrb::make(1));
        add_bottom(g, units::actions::AnimateOrb::make(1));
        add_bottom(g, units::actions::EvokeOrb::make(1));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.cost = 0; card.cost_for_turn = 0; card.upgraded_cost = true;
        }
    }
};
}
