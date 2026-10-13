#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Turnip : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300, false, false, PowerId::Frail}; }
};
}
