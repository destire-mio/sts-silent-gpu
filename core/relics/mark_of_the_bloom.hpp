#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MarkOfTheBloom : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 400}; d.prevents_revival = true; return d; }
    static SPIRE_FN int onPlayerHeal(Game&, int, int) { return 0; }
};
}
