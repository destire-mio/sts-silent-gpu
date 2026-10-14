#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Rupture : PowerHooks {
    static SPIRE_FN constexpr PowerData data() {
        PowerData d{};
        d.post_action = true;
        return d;
    }
    static SPIRE_FN void wasHPLost(Game& g, Power& p, int owner, const DamageInfo &info, int amount) {
        if (amount > 0 && info.source == owner) {
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.source = e.target = owner;
            e.power = PowerId::Strength;
            e.amount = p.amount;
            add_top(g, e);
        }
    }
};
}
