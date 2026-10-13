#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Constricted : PowerHooks {
    SPIRE_STATE(Constricted, int source;)
    static SPIRE_FN constexpr PowerData data() { return {105, true}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        Effect e{};
        e.kind = EffectKind::Damage;
        e.source = p.state.Constricted.source;
        e.target = owner;
        e.amount = p.amount;
        e.info_base = p.amount;
        e.damage_type = DamageType::Thorns;
        add_bottom(g, e);
    }
};
}
