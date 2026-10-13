#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct AccursedBlacksmith : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.AccursedBlacksmith", true}; }
};
}
