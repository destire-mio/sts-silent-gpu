#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Colosseum : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.city.Colosseum", true, true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.node_y > 7; }
};
}
