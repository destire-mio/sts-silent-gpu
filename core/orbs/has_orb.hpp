#pragma once
#include "../api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:hasOrb")

namespace spire {
SPIRE_FN inline bool has_orb(const Game& g) {
    return !g.orbs.slots.empty() && g.orbs.slots[0] >= 0;
}
}
