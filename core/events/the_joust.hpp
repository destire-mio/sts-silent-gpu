#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct TheJoust : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.city.TheJoust", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 && g.run.gold >= 50; }
};
}
