#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct QuestionCard : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
    static SPIRE_FN int changeNumberOfCardsInReward(Game&, int, int amount) { return amount + 1; }
};
}
