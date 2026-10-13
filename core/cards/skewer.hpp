#pragma once
#include "../hooks.hpp"
#include "../actions/skewer.hpp"

namespace spire::units::cards {
struct Skewer : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, -1, 7, -1, -1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        const Card& c = g.cards[card];
        add_bottom(g, actions::SkewerAction::construct(player_target, target, c.damage, DamageType::Normal, c.free_to_play_once, c.energy_on_use));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
