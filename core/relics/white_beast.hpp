#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct WhiteBeast : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 250}; d.potion_reward_chance = 100; return d; }
};
}
