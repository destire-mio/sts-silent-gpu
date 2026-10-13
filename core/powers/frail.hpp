#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Frail : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {10, true, false}; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.target = owner;
        e.power = PowerId::Frail;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float modifyBlock(const Game&, const Power&, int, float value, int = -1) {
        return value * 0.75f;
    }
};
}
