#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EndTurnDiscard : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect&) { end_turn_discard(g); }
};
}
