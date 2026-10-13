#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct ClockworkSouvenir : RelicHooks {
    static SPIRE_FN void atBattleStart(Game& g, int) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.power = PowerId::Artifact;
        e.amount = 1;
        add_top(g, e);
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
};
}
