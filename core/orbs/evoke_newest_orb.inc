#pragma once
#include "core/api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:evokeNewestOrb")

namespace spire {
SPIRE_WAITING_RULE("AbstractPlayer.evokeNewestOrb: no gameplay caller in the original Java")
SPIRE_FN inline void evoke_newest_orb(Game& g) {
    if (!g.orbs.slots.empty() && g.orbs.slots[g.orbs.slots.size - 1] >= 0)
        orb_evoke(g, g.orbs.slots[g.orbs.slots.size - 1]);
}
}
