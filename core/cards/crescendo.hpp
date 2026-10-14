#pragma once
#include "../hooks.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::cards {
struct Crescendo : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int, int) { add_bottom(g, units::actions::ChangeStance::make(Stance::Wrath)); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.cost = card.cost_for_turn = 0;
            card.upgraded_cost = true;
        }
    }
};
}
