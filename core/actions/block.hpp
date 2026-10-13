#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Block : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) { gain_block(g, e.target, e.amount); }
};
}
