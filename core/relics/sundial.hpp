#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::relics {
struct Sundial : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void onEquip(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void onShuffle(Game& g, int relic) {
        int& counter = g.run.relics[relic].counter;
        ++counter;
        if (counter == 3) {
            counter = 0;
            queue_relic_above(g, relic, player_target, false);
            // EmptyDeckShuffleAction's constructor calls onShuffle; GainEnergyAction has no constructor callbacks.
            const Effect e = actions::GainEnergy::make(2);
            if (g.room_phase == RoomPhase::Combat)
                g.manager.actions.push_back(g.action_objects.add(e));
        }
    }
};
}
