#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct GenericStrengthUp : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Strength;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
