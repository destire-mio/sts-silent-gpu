#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MarkOfTheBloom : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN int onPlayerHeal(Game&, int, int) { return 0; }
};
}
