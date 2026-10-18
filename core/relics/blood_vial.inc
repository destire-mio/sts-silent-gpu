#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct BloodVial : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, true);
        Effect e{};
        e.kind = EffectKind::Heal;
        e.amount = 2;
        add_top(g, e);
    }
};
}
