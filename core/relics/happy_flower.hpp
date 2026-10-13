#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::relics {
struct HappyFlower : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        int& counter = g.run.relics[relic].counter;
        counter += counter == -1 ? 2 : 1;
        if (counter == 3) {
            counter = 0;
            queue_relic_above(g, relic, player_target, false);
            add_bottom(g, units::actions::GainEnergy::make(1));
        }
    }
};
}
