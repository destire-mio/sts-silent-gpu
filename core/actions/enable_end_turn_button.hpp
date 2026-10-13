#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EnableEndTurnButton : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect&) { g.end_turn_enabled = true; }
};
}
