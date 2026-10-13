#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct GremlinMask : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        add_bottom(g, relic_above_creature(player_target, relic));
        queue_power(g, player_target, player_target, PowerId::Weak, 1);
    }
};
}
