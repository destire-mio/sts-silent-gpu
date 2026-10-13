#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BlackStar : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEnterRoom(Game&, int, int) {}
    static SPIRE_FN void onVictory(Game&, int) {}
};
}
