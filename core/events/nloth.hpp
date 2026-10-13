#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Nloth : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Nloth", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 && g.run.relics.size >= 2; }
};
}
