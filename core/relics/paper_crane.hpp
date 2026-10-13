#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct PaperCrane : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 250}; d.monster_weak_multiplier = 0.6f; return d; }
};
}
