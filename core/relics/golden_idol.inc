#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct GoldenIdol : RelicHooks {
    static constexpr float MULTIPLIER = 0.25f;
    static SPIRE_FN constexpr RelicData data() { RelicData value{-1, 400}; value.gold_reward_bonus = MULTIPLIER; return value; }
};
}
