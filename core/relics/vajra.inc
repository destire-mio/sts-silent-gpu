#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Vajra : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        Effect strength{};
        strength.kind = EffectKind::ApplyPower;
        strength.power = PowerId::Strength;
        strength.amount = 1;
        add_top(g, strength);
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_top(g, show);
    }
};
}
