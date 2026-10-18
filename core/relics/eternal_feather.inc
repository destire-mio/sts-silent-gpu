#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct EternalFeather : RelicHooks {
    static SPIRE_FN void onEnterRoom(Game& g, int, int room) {
        if (room == static_cast<int>(RoomKind::Rest)) heal_creature(g, player_target, g.run.master_deck.size / 5 * 3);
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
};
}
