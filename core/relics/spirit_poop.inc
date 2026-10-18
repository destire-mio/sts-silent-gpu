#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct SpiritPoop : RelicHooks {
    SPIRE_NOT_APPLICABLE("score: GameOverScreen.checkScoreBonus subtracts one point; score and score flags are outside the snapshot contract")
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
};
}
