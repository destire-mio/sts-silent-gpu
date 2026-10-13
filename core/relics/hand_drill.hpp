#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct HandDrill : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onBlockBroken(Game& g, int relic, int target) {
        add_bottom(g, relic_above_creature(target, relic));
        queue_power(g, player_target, target, PowerId::Vulnerable, 2);
    }
};
}
