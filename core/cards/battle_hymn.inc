#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct BattleHymn : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::Self, 1, -1, -1, 1, 0, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::BattleHymn, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.innate = true;
        }
    }
};
}
