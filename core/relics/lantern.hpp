#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::relics {
struct Lantern : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    SPIRE_STATE(Lantern, bool firstTurnUsed;)
    SPIRE_SNAPSHOT("Lantern.firstTurn", scalar, !relic.state.Lantern.firstTurnUsed)
    SPIRE_SNAPSHOT("AbstractRelic.energyBased", scalar, true)
    static SPIRE_FN void atPreBattle(Game& g, int relic) {
        g.run.relics[relic].state.Lantern.firstTurnUsed = false;
    }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        bool& used = g.run.relics[relic].state.Lantern.firstTurnUsed;
        if (!used) {
            add_top(g, units::actions::GainEnergy::make(1));
            add_top(g, relic_above_creature(player_target, relic));
            used = true;
        }
    }
};
}
