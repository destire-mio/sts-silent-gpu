#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BagOfPreparation : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, false);
        Effect e{};
        e.kind = EffectKind::Draw;
        e.amount = 2;
        add_bottom(g, e);
    }
};
}
