#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::relics {
struct HoveringKite : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    SPIRE_STATE(HoveringKite, bool triggeredThisTurn;)
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        g.run.relics[relic].state.HoveringKite.triggeredThisTurn = false;
    }
    static SPIRE_FN void onManualDiscard(Game& g, int relic) {
        bool& triggered = g.run.relics[relic].state.HoveringKite.triggeredThisTurn;
        if (!triggered) {
            triggered = true;
            queue_relic_above(g, relic, player_target, false);
            add_bottom(g, units::actions::GainEnergy::make(1));
        }
    }
};
}
