#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct HandCheck : ActionHooks {
    static SPIRE_FN void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    }
};
}
