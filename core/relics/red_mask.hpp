#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct RedMask : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        for (int i = 0; i < g.monster_order.size; ++i)
            queue_power(g, player_target, g.monster_order[i], PowerId::Weak, 1);
    }
};
}
