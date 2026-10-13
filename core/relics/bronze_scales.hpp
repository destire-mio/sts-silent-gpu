#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BronzeScales : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.power = PowerId::Thorns;
        e.amount = 3;
        add_top(g, e);
    }
};
}
