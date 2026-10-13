#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Mushrooms : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.Mushrooms", false, true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.floor > 6; }
};
}
