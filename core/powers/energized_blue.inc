#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct EnergizedBlue : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game &, Power& p, int) { p.amount = min(999, p.amount); }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount = min(999, p.amount + amount); }
    static SPIRE_FN void onEnergyRecharge(Game& g, Power& p, int owner) {
        gain_energy(g, p.amount);
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = e.target = owner;
        e.power = PowerId::EnergizedBlue;
        add_bottom(g, e);
    }
};
}
