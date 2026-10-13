#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct KnowingSkull : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.city.KnowingSkull", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 && g.player.creature.hp > 12; }
};
}
