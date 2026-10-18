#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::relics {
struct SnakeRing : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_above(g, relic, player_target);
        add_bottom(g, units::actions::Draw::construct(2, true, -1, player_target));
    }
};
}
