#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MembershipCard : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150, false, false, false, false, false, false, 0.5f}; }
};
}
