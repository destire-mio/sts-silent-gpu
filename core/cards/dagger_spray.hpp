#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct DaggerSpray : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 4, -1, -1, 2, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
