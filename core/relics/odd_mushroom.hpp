#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct OddMushroom : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 400}; d.player_vulnerable_multiplier = 1.25f; return d; }
};
}
