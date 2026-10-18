#pragma once
#include "core/api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:hasEmptyOrb")

namespace spire {
SPIRE_WAITING_RULE("AbstractPlayer.hasEmptyOrb: FrozenCore is unavailable in the Silent pool; waiting for oracle evidence")
SPIRE_FN inline bool has_empty_orb(const Game& g) {
    for (int i = 0; i < g.orbs.slots.size; ++i) if (g.orbs.slots[i] < 0) return true;
    return false;
}
}
