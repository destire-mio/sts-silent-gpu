#pragma once
#include "../api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:increaseMaxOrbSlots")

namespace spire {
SPIRE_FN inline void increase_max_orb_slots(Game& g, int amount) {
    if (g.player.max_orbs == 10) return;
    g.player.max_orbs += amount;
    for (int i = 0; i < amount; ++i) g.orbs.slots.push_back(-1);
}
}
