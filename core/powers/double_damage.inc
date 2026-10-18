#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct DoubleDamage : PowerHooks {
    SPIRE_SNAPSHOT("DoubleDamagePower.justApplied", scalar, power.just_applied)
    SPIRE_STATE(DoubleDamage, bool just_applied;)
    static SPIRE_FN constexpr PowerData data() { return {6, false, false, true, false, false, false, true}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.state.DoubleDamage.just_applied = p.just_applied; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.state.DoubleDamage.just_applied) { p.state.DoubleDamage.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::DoubleDamage;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float atDamageGive(const Game&, const Power&, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value * 2.0f : value;
    }
};
}
