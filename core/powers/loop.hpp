#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Loop : PowerHooks {
    SPIRE_WAITING_RULE("LoopPower: waiting for a ported caller with three-seed oracle replay")
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
