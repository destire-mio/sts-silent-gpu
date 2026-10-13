#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct FaceOfCleric : RelicHooks {
    static SPIRE_FN void onVictory(Game& g, int) { increase_max_hp(g, 1); }
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
};
}
