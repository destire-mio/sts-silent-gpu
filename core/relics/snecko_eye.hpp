#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SneckoEye : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) { g.player.master_hand_size += 2; }
    static SPIRE_FN void onUnequip(Game& g, int) { g.player.master_hand_size -= 2; }
    static SPIRE_FN void atPreBattle(Game& g, int) {
        queue_power(g, player_target, player_target, PowerId::Confusion, -1);
    }
};
}
