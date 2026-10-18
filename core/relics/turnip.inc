#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Turnip : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 300}; d.blocked_debuff = PowerId::Frail; return d; }
};
}
