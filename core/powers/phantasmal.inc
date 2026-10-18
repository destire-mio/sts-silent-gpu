#pragma once
#include "core/hooks.hpp"
#include "core/actions/apply_power.hpp"

namespace spire::units::powers {
struct Phantasmal : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int owner) {
        Power damage{PowerId::DoubleDamage, 1};
        power_constructPower(g, damage, owner, owner, 0, -1);
        power_initFromSource(g, damage, owner, false);
        add_bottom(g, actions::ApplyPower::make(owner, owner, damage, p.amount));
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
