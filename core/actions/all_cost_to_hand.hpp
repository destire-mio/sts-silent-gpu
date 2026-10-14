#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AllCostToHand : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_SNAPSHOT("AllCostToHandAction.p", player)
    SPIRE_SNAPSHOT("AllCostToHandAction.costTarget", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.discard.size; ++i)
            if (g.cards[g.discard[i]].cost == e.extra || g.cards[g.discard[i]].free_to_play_once) {
                Effect a{};
                a.kind = EffectKind::DiscardToHand;
                a.source = a.target = no_target;
                a.card = g.discard[i];
                add_bottom(g, a);
            }
    }
};
}
