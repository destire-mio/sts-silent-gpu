#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct SecretPortal : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.SecretPortal", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 3 && g.run.playtime >= 800.0f; }
};
}
