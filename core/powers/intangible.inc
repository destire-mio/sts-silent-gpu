#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Intangible : PowerHooks {
    SPIRE_SNAPSHOT("IntangiblePower.justApplied", scalar, power.just_applied)
    static SPIRE_FN constexpr PowerData data() { return {75}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.just_applied = true; }
    static SPIRE_FN float atDamageFinalReceive(const Game&, const Power&, int, float value, DamageType, int = -1) {
        return value > 1.0f ? 1.0f : value;
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Intangible;
        e.amount = 1;
        add_bottom(g, e);
    }
};
}
