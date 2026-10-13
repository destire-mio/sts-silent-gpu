#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct DeadAdventurer : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.DeadAdventurer", false, true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.floor > 6; }
};
}
