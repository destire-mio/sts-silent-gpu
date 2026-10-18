#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct LockOn : PowerHooks {
    static SPIRE_FN constexpr PowerData data() {
        PowerData d{};
        d.debuff = true;
        d.turn_based = true;
        return d;
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::LockOn;
            add_bottom(g, e);
        } else {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = e.target = owner;
            e.power = PowerId::LockOn;
            e.amount = 1;
            add_bottom(g, e);
        }
    }
};
}
