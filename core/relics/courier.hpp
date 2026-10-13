#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Courier : RelicHooks {
    static constexpr float MULTIPLIER = 0.8f;
    static SPIRE_FN void onEnterRoom(Game&, int, int) {}
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        return g.run.floor <= 48 && g.room_kind != RoomKind::Shop;
    }
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 250}; d.shop_multiplier = 0.8f; d.restocks_shop = true; d.excluded_from_restock = true; return d; }
};
}
