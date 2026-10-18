#pragma once
#include "core/api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:evokeWithoutLosingOrb")

namespace spire {
SPIRE_FN inline void evoke_without_losing_orb(Game& g) {
    if (has_orb(g)) orb_evoke(g, g.orbs.slots[0]);
}
}
