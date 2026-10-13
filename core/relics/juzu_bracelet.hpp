#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct JuzuBracelet : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150, false, false, false, false, false, true}; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
