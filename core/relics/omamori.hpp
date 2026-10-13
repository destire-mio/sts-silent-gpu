#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Omamori : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {2, 150}; }
    static SPIRE_FN void setCounter(Game& g, int relic, int amount) {
        Relic& r = g.run.relics[relic];
        r.counter = amount;
        if (amount == 0) r.grayscale = r.used_up = true;
    }
    static SPIRE_FN void use(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        if (--r.counter == 0) setCounter(g, relic, 0);
    }
    static SPIRE_FN bool onCurseObtain(Game& g, int relic) {
        if (g.run.relics[relic].counter == 0) return false;
        use(g, relic);
        return true;
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
