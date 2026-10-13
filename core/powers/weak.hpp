#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Weak : PowerHooks {
    SPIRE_SNAPSHOT("WeakPower.justApplied", scalar, power.just_applied)
    static SPIRE_FN constexpr PowerData data() { PowerData data{99, true, false}; data.turn_based = true; return data; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.target = owner;
        e.source = owner;
        e.power = PowerId::Weak;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float atDamageGive(const Game&, const Power&, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value * 0.75f : value;
    }
};
}
