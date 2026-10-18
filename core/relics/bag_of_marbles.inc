#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct BagOfMarbles : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            queue_relic_above(g, relic, owner, false);
            queue_power(g, player_target, owner, PowerId::Vulnerable, 1);
        }
    }
};
}
