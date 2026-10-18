#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Block : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "BLOCK"}; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        gain_block(g, e.target, e.amount);
    }
};
}
