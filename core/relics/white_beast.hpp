#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct WhiteBeast : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250, false, false, PowerId::Count, 100}; }
};
}
