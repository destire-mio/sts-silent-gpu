#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Sling : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        if (g.elite_trigger) {
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.power = PowerId::Strength;
            e.amount = 2;
            add_top(g, e);
        }
    }
};
}
