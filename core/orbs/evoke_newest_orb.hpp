#pragma once
#include "../api.hpp"

namespace spire {
SPIRE_FN inline void evoke_newest_orb(Game& g) {
    if (!g.orbs.slots.empty() && g.orbs.slots[g.orbs.slots.size - 1] >= 0)
        orb_evoke(g, g.orbs.slots[g.orbs.slots.size - 1]);
}
}
