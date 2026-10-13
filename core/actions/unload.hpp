#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct UnloadAction : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.target = no_target; }
    SPIRE_FN static void execute(Game&, Effect& e) { e.done = true; }
};
}
