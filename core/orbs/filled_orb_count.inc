#pragma once
#include "core/api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:filledOrbCount")

namespace spire {
SPIRE_FN inline int filled_orb_count(const Game& g) {
    int count = 0;
    for (int i = 0; i < g.orbs.slots.size; ++i) if (g.orbs.slots[i] >= 0) ++count;
    return count;
}
}
