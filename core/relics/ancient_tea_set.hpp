#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct AncientTeaSet : RelicHooks {
    SPIRE_STATE(AncientTeaSet, bool firstTurn;)
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.AncientTeaSet.firstTurn = true; }
    static SPIRE_FN void atPreBattle(Game& g, int relic) { g.run.relics[relic].state.AncientTeaSet.firstTurn = true; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        if (r.state.AncientTeaSet.firstTurn) {
            if (r.counter == -2) {
                r.counter = -1;
                Effect e = units::actions::GainEnergy::make(2);
                add_top(g, e);
                queue_relic_display(g, relic, player_target, true);
            }
            r.state.AncientTeaSet.firstTurn = false;
        }
    }
    static SPIRE_FN void setCounter(Game& g, int relic, int counter) { g.run.relics[relic].counter = counter; }
    static SPIRE_FN void onEnterRestRoom(Game& g, int relic) { g.run.relics[relic].counter = -2; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
