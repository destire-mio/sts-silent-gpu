#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Loop : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        if (!g.orbs.slots.empty())
            for (int i = 0; i < p.amount; ++i) {
                orb_start_turn(g, g.orbs.slots[0]);
                orb_end_turn(g, g.orbs.slots[0]);
            }
    }
};
}
