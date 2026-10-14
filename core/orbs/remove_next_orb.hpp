#pragma once
#include "../api.hpp"

namespace spire {
SPIRE_FN inline void remove_next_orb(Game& g) {
    if (!has_orb(g)) return;
    // AbstractPlayer.removeNextOrb uses the positioned EmptyOrbSlot constructor (ID "Empty").
    g.orbs.slots.erase(0); g.orbs.slots.push_back(-2);
}
}
