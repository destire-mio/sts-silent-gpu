#pragma once
#include "../api.hpp"

namespace spire {
SPIRE_FN inline void evoke_orb(Game& g) {
    if (!has_orb(g)) return;
    orb_evoke(g, g.orbs.slots[0]);
    g.orbs.slots.erase(0); g.orbs.slots.push_back(-1);
}
}
