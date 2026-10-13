#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Anchor : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_display(g, relic, player_target, false);
        queue_block(g, player_target, 10);
        g.run.relics[relic].grayscale = true;
    }
    static SPIRE_FN void justEnteredRoom(Game& g, int relic, int) {
        g.run.relics[relic].grayscale = false;
    }
};
}
