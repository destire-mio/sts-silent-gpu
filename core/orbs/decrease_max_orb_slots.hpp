#pragma once
#include "../api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:decreaseMaxOrbSlots")

namespace spire {
SPIRE_FN inline void decrease_max_orb_slots(Game& g, int amount) {
    if (g.player.max_orbs <= 0) return;
    g.player.max_orbs = max(0, g.player.max_orbs - amount);
    if (!g.orbs.slots.empty()) g.orbs.slots.pop_back();
}
}
