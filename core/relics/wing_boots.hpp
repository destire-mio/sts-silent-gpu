#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct WingBoots : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{3, 300}; d.crosses_map_paths = true; return d; }
    static SPIRE_FN void setCounter(Game& g, int relic, int counter) {
        Relic& boots = g.run.relics[relic];
        boots.counter = counter;
        if (counter == -2) {
            boots.used_up = true;
            boots.grayscale = true;
        }
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 40; }
};
}
