#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct IceCream : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300, false, false, true}; }
};
}
