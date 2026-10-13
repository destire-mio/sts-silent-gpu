#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct TinyChest : RelicHooks {
    SPIRE_WAITING_RULE("EventHelper.roll: every fourth event becomes a chest")
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 35; }
};
}
