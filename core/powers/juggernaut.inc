#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Juggernaut : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onGainedBlock(Game& g, Power& p, int owner, float amount) {
        if (amount > 0.0f) {
            Effect e{};
            e.kind = EffectKind::DamageRandomEnemy;
            e.source = owner;
            e.target = no_target;
            e.info_base = e.amount = p.amount;
            e.damage_type = DamageType::Thorns;
            add_bottom(g, e);
        }
    }
};
}
