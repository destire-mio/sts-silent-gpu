#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct RunicPyramid : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 999}; d.keep_hand = true; return d; }
};
}
