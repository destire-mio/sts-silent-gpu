#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Phantasmal : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power&, int owner) {
        Effect e{};
        e.kind = EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Phantasmal;
        e.amount = 1;
        add_bottom(g, e);
    }
};
}
