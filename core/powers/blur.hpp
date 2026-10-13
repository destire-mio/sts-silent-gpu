#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Blur : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, false, true, false, false, false, true}; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Blur;
        e.amount = 1;
        add_bottom(g, e);
    }
};
}
