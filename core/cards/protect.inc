#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Protect : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 2, -1, 12, -1, 0, 4, 0, false, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
