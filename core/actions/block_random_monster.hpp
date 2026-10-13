#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct BlockRandomMonster : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect& e) { block_random_monster(g, e); }
};
}
