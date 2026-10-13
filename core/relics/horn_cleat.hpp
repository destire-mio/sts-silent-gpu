#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct HornCleat : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    SPIRE_STATE(HornCleat, bool grayscale;)
    SPIRE_SNAPSHOT("AbstractRelic.grayscale", scalar, relic.state.HornCleat.grayscale)
    static SPIRE_FN void atBattleStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        if (!r.state.HornCleat.grayscale) ++r.counter;
        if (r.counter == 2) {
            queue_relic_above(g, relic, player_target, false);
            queue_block(g, player_target, 14);
            r.counter = -1;
            r.state.HornCleat.grayscale = true;
        }
    }
    static SPIRE_FN void onVictory(Game& g, int relic) {
        g.run.relics[relic].counter = -1;
        g.run.relics[relic].state.HornCleat.grayscale = false;
    }
};
}
