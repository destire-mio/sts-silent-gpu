#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct FlameBarrier : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo &info, int amount) {
        if (info.source != no_target && info.source != owner && info.damage_type == DamageType::Normal) {
            Effect e{};
            e.kind = EffectKind::Damage;
            e.source = owner;
            e.target = info.source;
            e.amount = e.info_base = p.amount;
            e.damage_type = DamageType::Thorns;
            add_top(g, e);
        }
        return amount;
    }
    static SPIRE_FN void atStartOfTurn(Game& g, Power &, int) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = e.target = player_target;
        e.power = PowerId::FlameBarrier;
        add_bottom(g, e);
    }
};
}
