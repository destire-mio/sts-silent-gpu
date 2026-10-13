#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct NoBlock : PowerHooks {
    SPIRE_SNAPSHOT("NoBlockPower.justApplied", scalar, power.just_applied)
    static SPIRE_FN constexpr PowerData data() { PowerData data{5, true}; data.turn_based = true; return data; }
    static SPIRE_FN void initFromSource(Game& g, Power& p, int, bool monster_source) {
        p.just_applied = monster_source && g.manager.turn_has_ended;
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::NoBlock;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float modifyBlockLast(const Game&, const Power&, int, float) { return 0.0f; }
};
}
