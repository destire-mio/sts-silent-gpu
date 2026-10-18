#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct OddlySmoothStone : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.power = PowerId::Dexterity;
        e.amount = 1;
        add_top(g, e);
        queue_relic_above(g, relic, player_target, true);
    }
};
}
