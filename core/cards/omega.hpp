#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Omega : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::Self, 3, -1, -1, 50, 0, 0, 10, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Omega, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 10;
            card.magic = card.base_magic;
        }
    }
};
}
