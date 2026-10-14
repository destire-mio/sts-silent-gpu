#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct FeelNoPain : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::Self, 1, -1, -1, 3, 0, 0, 1, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::FeelNoPain, g.cards[card].magic, false, 0);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
