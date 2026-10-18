#pragma once
#include "core/api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:switchedStance")

namespace spire {
SPIRE_FN inline void switched_stance(Game& g) {
    for (int i = next_card_switchedStance(g, g.hand, 0); i < g.hand.size; i = next_card_switchedStance(g, g.hand, i + 1)) card_switchedStance(g, g.hand[i]);
    for (int i = next_card_switchedStance(g, g.discard, 0); i < g.discard.size; i = next_card_switchedStance(g, g.discard, i + 1)) card_switchedStance(g, g.discard[i]);
    for (int i = next_card_switchedStance(g, g.draw, 0); i < g.draw.size; i = next_card_switchedStance(g, g.draw, i + 1)) card_switchedStance(g, g.draw[i]);
}
}
