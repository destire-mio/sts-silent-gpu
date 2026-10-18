#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::relics {
struct BagOfPreparation : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, false);
        add_bottom(g, actions::Draw::construct(2, true, -1, player_target));
    }
};
}
