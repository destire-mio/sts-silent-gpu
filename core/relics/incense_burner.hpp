#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct IncenseBurner : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onEquip(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        int& counter = g.run.relics[relic].counter;
        counter += counter == -1 ? 2 : 1;
        if (counter == 6) {
            counter = 0;
            add_bottom(g, relic_above_creature(player_target, relic));
            queue_power(g, no_target, player_target, PowerId::IntangiblePlayer, 1);
        }
    }
};
}
