#pragma once
#include "../hooks.hpp"
#include "../actions/obtain_potion.hpp"

namespace spire::units::cards {
struct Alchemize : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        const PotionId potion = random_potion(g, true);
        add_bottom(g, actions::ObtainPotion::from_potion(potion, construct_potion(g, potion)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
