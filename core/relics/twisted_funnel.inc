#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct TwistedFunnel : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            const Creature& monster = g.monsters[target].creature;
            if (!monster.dead && !monster.dying) queue_power(g, player_target, target, PowerId::Poison, 4);
        }
    }
};
}
