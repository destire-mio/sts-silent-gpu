#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SelfRepair : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, 7, 0, 0, 3, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Repair, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 3;
            card.magic = card.base_magic;
        }
    }
};
}
