#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MembershipCard : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.shop_multiplier = 0.5f; return d; }
};
}
