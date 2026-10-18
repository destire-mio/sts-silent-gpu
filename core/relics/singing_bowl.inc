#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct SingingBowl : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 250}; d.bowl_hp = 2; return d; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
