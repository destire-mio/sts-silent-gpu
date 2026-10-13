#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct CripplingPoison : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::AllEnemies, 2, -1, -1, 4, 0, 0, 3, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        if (!monsters_basically_dead(g)) {
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int owner = g.monster_order[i];
                const Creature& monster = g.monsters[owner].creature;
                if (monster.dead || monster.dying) continue;
                queue_power(g, player_target, owner, PowerId::Poison, g.cards[card].magic);
                queue_power(g, player_target, owner, PowerId::Weak, 2);
            }
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
