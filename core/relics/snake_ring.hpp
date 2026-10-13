#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::relics {
struct SnakeRing : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        add_bottom(g, units::actions::Draw::construct(2));
    }
};
}
