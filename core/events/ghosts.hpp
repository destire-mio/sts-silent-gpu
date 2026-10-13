#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Ghosts : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.city.Ghosts", true}; }
};
}
