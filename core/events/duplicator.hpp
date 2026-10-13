#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Duplicator : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Duplicator", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 || g.run.act == 3; }
};
}
