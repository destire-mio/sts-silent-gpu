#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BlackStar : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 999}; d.extra_elite_relic = true; return d; }
    static SPIRE_FN void onEnterRoom(Game&, int, int) {}
    static SPIRE_FN void onVictory(Game&, int) {}
};
}
