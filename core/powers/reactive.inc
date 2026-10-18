#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Reactive : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {50}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
    static SPIRE_FN int onAttacked(Game& g, Power&, int owner, const DamageInfo& info, int amount) {
        if (info.source != no_target && info.damage_type != DamageType::HpLoss &&
            info.damage_type != DamageType::Thorns && amount > 0 && amount < get_creature(g, owner).hp) {
            Effect e{};
            e.kind = EffectKind::RollMove;
            e.target = owner;
            add_bottom(g, e);
        }
        return amount;
    }
};
}
