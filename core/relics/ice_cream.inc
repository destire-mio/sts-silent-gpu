#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct IceCream : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 300}; d.preserves_energy = true; return d; }
};
}
