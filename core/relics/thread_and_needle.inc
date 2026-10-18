#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct ThreadAndNeedle : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.power = PowerId::PlatedArmor;
        e.amount = 4;
        add_top(g, e);
    }
};
}
