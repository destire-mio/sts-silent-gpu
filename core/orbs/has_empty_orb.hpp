#pragma once
#include "../api.hpp"

namespace spire {
SPIRE_FN inline bool has_empty_orb(const Game& g) {
    for (int i = 0; i < g.orbs.slots.size; ++i) if (g.orbs.slots[i] < 0) return true;
    return false;
}
}
