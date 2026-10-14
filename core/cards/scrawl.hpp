#pragma once
#include "../hooks.hpp"
#include "../actions/expertise.hpp"

namespace spire::units::cards {
struct Scrawl : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, actions::Expertise::construct(player_target, 10));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
