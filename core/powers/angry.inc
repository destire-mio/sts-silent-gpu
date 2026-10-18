#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Angry : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { PowerData data{5, false, false}; data.post_action = true; return data; }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo& info, int amount) {
        if (info.source != no_target && amount > 0 && info.damage_type != DamageType::HpLoss &&
            info.damage_type != DamageType::Thorns) {
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Strength;
            e.amount = p.amount;
            add_top(g, e);
        }
        return amount;
    }
};
}
