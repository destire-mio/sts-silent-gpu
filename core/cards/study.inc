#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Study : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::Self, 2, -1, -1, 1, 0, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Study, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            const int difference = card.cost_for_turn - card.cost;
            card.cost = 1;
            if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + difference;
            if (card.cost_for_turn < 0) card.cost_for_turn = 0;
            card.upgraded_cost = true;
        }
    }
};
}
