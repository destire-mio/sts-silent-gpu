#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct FrozenEye : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
};
}
