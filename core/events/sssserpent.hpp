#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Sssserpent : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.Sssserpent", true}; }
};
}
