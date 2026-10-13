#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SmilingMask : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.purge_cost = 50; return d; }
    SPIRE_STATE(SmilingMask, bool pulse;)
    static SPIRE_FN void onEnterRoom(Game& g, int relic, int room) {
        g.run.relics[relic].state.SmilingMask.pulse = static_cast<RoomType>(room) == RoomType::Shop;
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        return g.run.floor <= 48 && g.room_type != RoomType::Shop;
    }
};
}
