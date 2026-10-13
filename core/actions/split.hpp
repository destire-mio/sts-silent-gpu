#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Split : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { split_monster(g, e.target); }
};
}
