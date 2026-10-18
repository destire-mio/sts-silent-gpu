#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DoubleYourBlock : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target != no_target && get_creature(g, e.target).block > 0)
            gain_block(g, e.target, get_creature(g, e.target).block);
    }
};
}
