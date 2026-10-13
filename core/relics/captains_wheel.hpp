#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct CaptainsWheel : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        if (!r.grayscale) ++r.counter;
        if (r.counter == 3) {
            queue_relic_display(g, relic, player_target, false);
            queue_block(g, player_target, 18);
            r.counter = -1;
            r.grayscale = true;
        }
    }
    static SPIRE_FN void onVictory(Game& g, int relic) {
        g.run.relics[relic].counter = -1;
        g.run.relics[relic].grayscale = false;
    }
};
}
