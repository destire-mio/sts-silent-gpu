#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EnableEndTurn : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game&, Effect& e) { e.done = true; }
};
}
