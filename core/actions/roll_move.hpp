#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RollMove : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { roll_monster_move(g, e.target); }
};
}
