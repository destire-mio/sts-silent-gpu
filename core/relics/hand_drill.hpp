#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct HandDrill : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onBlockBroken(Game& g, int relic, int target) {
        queue_relic_above(g, relic, target, false);
        queue_power(g, player_target, target, PowerId::Vulnerable, 2);
    }
};
}
