#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct PotionSlot : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {0, CardTarget::Self, false}; }
    static SPIRE_FN void use(Game&, int, int) {}
};
}
