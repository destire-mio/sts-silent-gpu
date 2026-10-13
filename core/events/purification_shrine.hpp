#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct PurificationShrine : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.PurificationShrine", true}; }
};
}
