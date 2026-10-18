#pragma once
#include "core/api.hpp"

namespace spire {
SPIRE_FN inline void end_turn_orbs(Game& g) {
    for (int i = 0; i < g.orbs.slots.size; ++i) orb_end_turn(g, g.orbs.slots[i]);
}
}
