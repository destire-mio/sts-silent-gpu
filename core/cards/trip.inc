#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Trip : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 0, -1, -1, 2, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (g.cards[card].upgrades == 0) queue_power(g, player_target, target, PowerId::Vulnerable, g.cards[card].magic);
        else for (int i = 0; i < g.monster_order.size; ++i)
            queue_power(g, player_target, g.monster_order[i], PowerId::Vulnerable, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.target = CardTarget::AllEnemies;
        }
    }
};
}
