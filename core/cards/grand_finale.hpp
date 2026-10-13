#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct GrandFinale : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 0, 50, -1, -1, 10, 0, 0, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
    }
    static SPIRE_FN bool canUse(const Game& g, int card, int target) {
        return card_can_use_base(g, card, target) && g.draw.empty();
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
