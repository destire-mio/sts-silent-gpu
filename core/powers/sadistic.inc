#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Sadistic : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onApplyPower(Game& g, Power& p, int owner, const Power& applied, int target, int source) {
        if (power_isDebuff(applied, target) && applied.id != PowerId::GainStrength && source == owner &&
            target != owner && !has_power(g, get_creature(g, target), PowerId::Artifact)) {
            Effect e{};
            e.kind = EffectKind::Damage;
            e.source = owner;
            e.target = target;
            e.amount = p.amount;
            e.info_base = p.amount;
            e.damage_type = DamageType::Thorns;
            add_bottom(g, e);
        }
    }
};
}
