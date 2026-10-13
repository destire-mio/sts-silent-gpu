#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MealTicket : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void justEnteredRoom(Game& g, int relic, int room) {
        if (room == static_cast<int>(RoomKind::Shop)) {
            if (g.room_in_combat) queue_relic_above(g, relic, player_target, true);
            heal_creature(g, player_target, 15);
        }
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
