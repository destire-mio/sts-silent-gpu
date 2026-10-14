#pragma once
#include "../api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:applyStartOfTurnOrbs")

namespace spire {
SPIRE_FN inline void start_turn_orbs(Game& g) {
    for (int i = 0; i < g.orbs.slots.size; ++i) orb_start_turn(g, g.orbs.slots[i]);
    if (has_relic(g, "Cables") && has_orb(g)) orb_start_turn(g, g.orbs.slots[0]);
}
}
