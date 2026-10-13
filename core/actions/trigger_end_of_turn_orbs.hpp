#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct TriggerEndOfTurnOrbs : ActionHooks {
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
}
