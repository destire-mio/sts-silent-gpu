#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct RingOfTheSerpent : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 999}; d.replaces_starter = true; return d; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.master_hand_size; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.master_hand_size; }
    static SPIRE_FN void atTurnStart(Game&, int) {}
    static SPIRE_FN bool canSpawn(const Game& g, int) { return has_relic(g, RelicId::SnakeRing); }
};
}
