#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Ginger : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData value{-1, 300}; value.blocked_debuff = PowerId::Weak; return value; }
};
}
