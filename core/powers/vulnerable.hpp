#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Vulnerable : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true, false}; }
    static SPIRE_FN void initFromSource(Game& g, Power& p, int, bool monster_source) {
        p.just_applied = monster_source && g.manager.turn_has_ended;
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.target = owner;
        e.power = PowerId::Vulnerable;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float atDamageReceive(const Game&, const Power&, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value * 1.5f : value;
    }
};
}
