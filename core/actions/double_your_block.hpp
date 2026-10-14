#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DoubleYourBlock : ActionHooks {
    SPIRE_WAITING_RULE("DoubleYourBlockAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (get_creature(g, e.target).block > 0)
            gain_block(g, e.target, get_creature(g, e.target).block);
    }
};
}
