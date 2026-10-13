#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SsserpentHead : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void onEnterRoom(Game& g, int, int room) {
        if (static_cast<RoomType>(room) == RoomType::Event) gain_gold(g, 50);
    }
};
}
