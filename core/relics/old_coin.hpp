#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct OldCoin : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onEquip(Game& g, int) { gain_gold(g, 300); }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48 && !g.room_is_shop; }
};
}
