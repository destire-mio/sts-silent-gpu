#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct DrawReduction : PowerHooks {
    SPIRE_SNAPSHOT("DrawReductionPower.justApplied", scalar, power.state.DrawReduction.just_applied)
    SPIRE_STATE(DrawReduction, bool just_applied;)
    static SPIRE_FN constexpr PowerData data() { return {5, true, false, true, false, false, false, true}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.state.DrawReduction.just_applied = true; }
    static SPIRE_FN void onInitialApplication(Game& g, Power&, int) { --g.player.hand_size; }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.state.DrawReduction.just_applied) { p.state.DrawReduction.just_applied = false; return; }
        Effect e{};
        e.kind = EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::DrawReduction;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void onRemove(Game& g, Power&, int) { ++g.player.hand_size; }
};
}
