#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Slow : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void atEndOfRound(Game&, Power& p, int) { p.amount = 0; }
    static SPIRE_FN void onAfterUseCard(Game& g, Power&, int owner, int, Effect&) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Slow;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float atDamageReceive(const Game&, const Power& p, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value * (1.0f + static_cast<float>(p.amount) * 0.1f) : value;
    }
};
}
