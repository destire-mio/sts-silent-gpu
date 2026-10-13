#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct RegalPillow : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.rest_heal_bonus = 15; return d; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
