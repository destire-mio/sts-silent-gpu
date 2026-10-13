#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct GoopPuddle : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.GoopPuddle", true}; }
};
}
