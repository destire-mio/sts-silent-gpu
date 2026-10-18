#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct Hyperbeam : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 2, 26, -1, 3, 8, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, units::actions::DamageAllEnemies::from_card(g, card));
        queue_power(g, player_target, player_target, PowerId::Focus, -g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 8;
        }
    }
};
}
