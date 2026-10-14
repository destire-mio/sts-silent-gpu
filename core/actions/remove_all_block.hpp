#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RemoveAllBlock : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        Creature &c = get_creature(g, e.target);
        if (!c.dying && !c.dead && c.block > 0)
            c.block = 0;
    }
};
}
