#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Caltrops : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, 3, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Thorns, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 2;
            card.magic = card.base_magic;
        }
    }
};
}
