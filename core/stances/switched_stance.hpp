#pragma once
#include "../api.hpp"

namespace spire {
SPIRE_FN inline void switched_stance(Game& g) {
    for (int i = 0; i < g.hand.size; ++i) card_switchedStance(g, g.hand[i]);
    for (int i = 0; i < g.discard.size; ++i) card_switchedStance(g, g.discard[i]);
    for (int i = 0; i < g.draw.size; ++i) card_switchedStance(g, g.draw[i]);
}
}
