#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct StrangeSpoon : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.exhaust_discard = true; return d; }
};
}
