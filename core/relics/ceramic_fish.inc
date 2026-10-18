#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct CeramicFish : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void use(Game& g, int relic) { --g.run.relics[relic].counter; }
    static SPIRE_FN void onObtainCard(Game& g, int, Card&) { gain_gold(g, 9); }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
