#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Envenom : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onAttack(Game& g, Power& p, int owner, const DamageInfo& info, int amount, int target) {
        if (amount > 0 && target != owner && info.damage_type == DamageType::Normal) {
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.source = owner;
            e.target = target;
            e.power = PowerId::Poison;
            e.amount = p.amount;
            add_top(g, e);
        }
    }
};
}
