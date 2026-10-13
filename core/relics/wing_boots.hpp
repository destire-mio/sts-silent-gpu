#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct WingBoots : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {3, 300}; }
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
