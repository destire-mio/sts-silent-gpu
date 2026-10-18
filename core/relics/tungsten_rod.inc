#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct TungstenRod : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN int onLoseHpLast(Game&, int, int amount) { return amount > 0 ? amount - 1 : amount; }
};
}
