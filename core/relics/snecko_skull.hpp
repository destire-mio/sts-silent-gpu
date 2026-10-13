#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SneckoSkull : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.poison_bonus = 1; return d; }
};
}
