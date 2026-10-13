#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Block : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.duration = e.start_duration = 0.25f; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        gain_block(g, e.target, e.amount);
        finish_action_duration(e);
    }
};
}
