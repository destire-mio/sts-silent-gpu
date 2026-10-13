#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SpiritPoop : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 400}; d.score = -1; return d; }
};
}
