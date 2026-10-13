#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Fading : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void duringTurn(Game& g, Power& p, int owner) {
        if (p.amount == 1 && !get_creature(g, owner).dying) return;
        Effect e{};
        e.kind = EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Fading;
        e.amount = 1;
        add_bottom(g, e);
    }
};
}
