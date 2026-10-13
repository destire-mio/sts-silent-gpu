#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Designer : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Designer", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return (g.run.act == 2 || g.run.act == 3) && g.run.gold >= 75; }
};
}
