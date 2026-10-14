#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Barricade : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 3, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        if (!has_power(g, g.player.creature, PowerId::Barricade))
            queue_power(g, player_target, player_target, PowerId::Barricade, -1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_card_base_cost(card, 2);
        }
    }
};
}
