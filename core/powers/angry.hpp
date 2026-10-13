#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Angry : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, false}; }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo& info, int amount) {
        if (info.source != no_target && amount > 0 && info.damage_type != DamageType::HpLoss &&
            info.damage_type != DamageType::Thorns && !monsters_basically_dead(g)) {
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
