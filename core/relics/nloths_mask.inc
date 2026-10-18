#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct NlothsMask : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {1, 400}; }
    static SPIRE_FN void setCounter(Game& g, int relic, int amount) {
        Relic& r = g.run.relics[relic];
        r.counter = amount;
        if (amount == -2) r.grayscale = r.used_up = true;
    }
    static SPIRE_FN void onChestOpenAfter(Game& g, int relic, bool boss_chest) {
        Relic& r = g.run.relics[relic];
        if (boss_chest || r.counter <= 0) return;
        --r.counter;
        remove_room_relic_reward(g);
        if (r.counter == 0) setCounter(g, relic, -2);
    }
};
}
