#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct MawBank : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.excluded_from_restock = true; return d; }
    SPIRE_STATE(MawBank, bool usedUp;)
    SPIRE_SNAPSHOT("AbstractRelic.usedUp", scalar, relic.state.MawBank.usedUp)
    SPIRE_SNAPSHOT("AbstractRelic.grayscale", scalar, relic.state.MawBank.usedUp)
    static SPIRE_FN void setCounter(Game& g, int relic, int value) {
        g.run.relics[relic].counter = value;
        if (value == -2) g.run.relics[relic].state.MawBank.usedUp = true;
    }
    static SPIRE_FN void onEnterRoom(Game& g, int relic, int) {
        if (!g.run.relics[relic].state.MawBank.usedUp) gain_gold(g, 12);
    }
    static SPIRE_FN void onSpendGold(Game& g, int relic) {
        if (!g.run.relics[relic].state.MawBank.usedUp) setCounter(g, relic, -2);
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) {
        return g.run.floor <= 48 && g.room_kind != RoomKind::Shop;
    }
};
}
