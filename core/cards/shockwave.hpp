#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Shockwave : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::AllEnemies, 2, -1, -1, 3, 0, 0, 2, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            queue_power(g, player_target, owner, PowerId::Weak, g.cards[card].magic);
            queue_power(g, player_target, owner, PowerId::Vulnerable, g.cards[card].magic);
        }
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
