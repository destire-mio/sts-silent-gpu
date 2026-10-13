#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct NlothsGift : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN int changeRareCardRewardChance(Game&, int, int amount) { return amount * 3; }
};
}
