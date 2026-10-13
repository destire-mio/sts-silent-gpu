#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct PrayerWheel : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 300}; d.extra_normal_combat_card_reward = true; return d; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
