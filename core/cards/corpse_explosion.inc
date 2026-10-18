#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct CorpseExplosion : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 2, -1, -1, 6, 0, 0, 3, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_power(g, player_target, target, PowerId::Poison, g.cards[card].magic);
        queue_power(g, player_target, target, PowerId::CorpseExplosion, 1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
