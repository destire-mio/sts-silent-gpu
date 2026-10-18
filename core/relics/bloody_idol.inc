#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct BloodyIdol : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void onGainGold(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, true);
        heal_creature(g, player_target, 5);
    }
};
}
