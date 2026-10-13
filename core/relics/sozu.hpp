#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Sozu : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 999}; d.blocks_potions = true; return d; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
};
}
