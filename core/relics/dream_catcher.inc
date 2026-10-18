#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct DreamCatcher : RelicHooks {
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.rest_card_reward = true; return d; }
};
}
