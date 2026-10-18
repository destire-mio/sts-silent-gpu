#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct PressEndTurnButton : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect &) { end_turn_early(g); }
};
}
