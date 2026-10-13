#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct DreamCatcher : RelicHooks {
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
};
}
