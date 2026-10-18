#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct LoseStrength : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true, false}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        Effect strength{};
        strength.kind = EffectKind::ApplyPower;
        strength.source = owner;
        strength.target = owner;
        strength.power = PowerId::Strength;
        strength.amount = -p.amount;
        add_bottom(g, strength);
        Effect remove{};
        remove.kind = EffectKind::RemovePower;
        remove.source = owner;
        remove.target = owner;
        remove.power = PowerId::LoseStrength;
        add_bottom(g, remove);
    }
};
}
