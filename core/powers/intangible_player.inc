#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct IntangiblePlayer : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { PowerData data{75}; data.incoming_damage_limit = 1; return data; }
    static SPIRE_FN float atDamageFinalReceive(const Game&, const Power&, int, float value, DamageType, int = -1) {
        return value > 1.0f ? 1.0f : value;
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::IntangiblePlayer;
        e.amount = 1;
        add_bottom(g, e);
    }
};
}
