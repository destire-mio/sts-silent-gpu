#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Pocketwatch : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    SPIRE_STATE(Pocketwatch, bool firstTurn;)
    static SPIRE_FN void initializeState(Relic& r) { r.state.Pocketwatch.firstTurn = true; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        r.counter = 0;
        r.state.Pocketwatch.firstTurn = true;
    }
    static SPIRE_FN void atTurnStartPostDraw(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        if (r.counter <= 3 && !r.state.Pocketwatch.firstTurn) {
            Effect e{};
            e.kind = EffectKind::Draw;
            e.amount = 3;
            add_bottom(g, e);
        } else r.state.Pocketwatch.firstTurn = false;
        r.counter = 0;
    }
    static SPIRE_FN void onPlayCard(Game& g, int relic, int, int) { ++g.run.relics[relic].counter; }
    static SPIRE_FN void onVictory(Game& g, int relic) { g.run.relics[relic].counter = -1; }
};
}
