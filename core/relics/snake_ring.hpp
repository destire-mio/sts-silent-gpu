#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SnakeRing : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.amount = 2;
        add_bottom(g, draw);
    }
};
}
