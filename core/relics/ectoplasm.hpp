#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Ectoplasm : RelicHooks {
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.act <= 1; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
    static SPIRE_FN constexpr RelicData data() { RelicData value{-1, 999}; value.blocks_gold = true; return value; }
};
}
