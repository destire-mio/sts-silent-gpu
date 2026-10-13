#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct RingOfTheSerpent : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.master_hand_size; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.master_hand_size; }
    static SPIRE_FN void atTurnStart(Game&, int) {}
    static SPIRE_FN bool canSpawn(const Game& g, int) { return has_relic(g, "Ring of the Snake"); }
};
}
