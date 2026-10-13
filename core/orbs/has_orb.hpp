#pragma once
#include "../api.hpp"

namespace spire {
SPIRE_FN inline bool has_orb(const Game& g) {
    return !g.orbs.slots.empty() && g.orbs.slots[0] >= 0;
}
}
