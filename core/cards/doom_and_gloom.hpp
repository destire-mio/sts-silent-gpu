#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"
#include "../actions/channel.hpp"
namespace spire::units::cards {
struct DoomAndGloom : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 2, 10, -1, 1, 4, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
        add_bottom(g, actions::Channel::make(make_orb(g, OrbKind::Dark)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
