#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Explosive : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void duringTurn(Game& g, Power& p, int owner) {
        Effect e{};
        e.source = owner;
        if (p.amount == 1 && !get_creature(g, owner).dying) {
            Effect suicide{};
            suicide.kind = EffectKind::Suicide;
            suicide.target = owner;
            add_bottom(g, suicide);
            // ExplosivePower.duringTurn constructs DamageInfo without applyPowers.
            e.kind = EffectKind::Damage;
            e.target = player_target;
            e.amount = 30;
            e.flag = true;
            e.info_base = 30;
            e.damage_type = DamageType::Thorns;
        } else {
            e.kind = EffectKind::ReducePower;
            e.target = owner;
            e.power = PowerId::Explosive;
            e.amount = 1;
        }
        add_bottom(g, e);
    }
};
}
