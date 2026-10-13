#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct BlockRandomMonster : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { block_random_monster(g, e); }
};
}
