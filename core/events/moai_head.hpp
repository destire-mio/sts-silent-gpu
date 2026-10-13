#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct MoaiHead : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.MoaiHead", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return has_relic(g, "Golden Idol") || static_cast<float>(g.player.creature.hp) / g.player.creature.max_hp <= 0.5f; }
};
}
