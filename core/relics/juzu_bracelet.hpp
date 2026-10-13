#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct JuzuBracelet : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.prevents_event_combat = true; return d; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
