#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Frail : PowerHooks {
    SPIRE_SNAPSHOT("FrailPower.justApplied", scalar, power.just_applied)
    static SPIRE_FN constexpr PowerData data() { PowerData data{10, true, false}; data.turn_based = true; return data; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.target = owner;
        e.source = owner;
        e.power = PowerId::Frail;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float modifyBlock(const Game&, const Power&, int, float value, int = -1) {
        return value * 0.75f;
    }
};
}
