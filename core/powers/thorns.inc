#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Thorns : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) {
        if (p.amount != -1) p.amount += amount;
    }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo& info, int amount) {
        if (info.damage_type != DamageType::Thorns && info.damage_type != DamageType::HpLoss &&
            info.source != no_target && info.source != owner) {
            Effect e{};
            e.kind = EffectKind::Damage;
            e.source = owner;
            e.target = info.source;
            e.amount = e.info_base = p.amount;
            e.damage_type = DamageType::Thorns;
            e.flag = true;
            add_top(g, e);
        }
        return amount;
    }
};
}
