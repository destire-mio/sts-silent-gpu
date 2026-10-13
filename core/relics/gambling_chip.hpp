#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct GamblingChip : RelicHooks {
    SPIRE_STATE(GamblingChip, bool activated;)
    static SPIRE_FN void atBattleStartPreDraw(Game& g, int relic) { g.run.relics[relic].state.GamblingChip.activated = false; }
    static SPIRE_FN void atTurnStartPostDraw(Game& g, int relic) {
        if (!g.run.relics[relic].state.GamblingChip.activated) {
            g.run.relics[relic].state.GamblingChip.activated = true;
            queue_relic_above(g, relic, player_target, false);
            Effect e{};
            e.kind = EffectKind::GamblingChipAction;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
};
}
