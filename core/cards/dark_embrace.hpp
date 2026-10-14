#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct DarkEmbrace : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::Self, 2, -1, -1, -1, 0, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int, int) {
        queue_power(g, player_target, player_target, PowerId::DarkEmbrace, 1, false, 0);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.cost = card.cost_for_turn = 1;
            card.upgraded_cost = true;
        }
    }
};
}
