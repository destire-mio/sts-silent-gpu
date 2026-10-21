#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Block : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "BLOCK"}; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        gain_block(g, e.target, e.amount);
        // GainBlockAction.update grants block first, then ticks 0.25s (XFAST for superFast).
        settle_deaths_after_wait(g, e.flag ? 7 : 16);
    }
};
}
