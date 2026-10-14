#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ReduceCost : ActionHooks {
    SPIRE_WAITING_RULE("ReduceCostAction: waiting for a ported caller with three-seed oracle replay")
    SPIRE_SNAPSHOT("ReduceCostAction.card", card, effect.card)
    SPIRE_STATE(ReduceCost, uint32_t uuid;)
    SPIRE_SNAPSHOT("ReduceCostAction.uuid", uuid, effect.payload.state.ReduceCost.uuid)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.card >= 0) {
            modify_cost_for_combat(g.cards[e.card], -1);
            return;
        }
        bool found[card_capacity]{};
        if (g.player.card_in_use >= 0)
            found[g.player.card_in_use] = true;
        const Pile *piles[] = {&g.hand, &g.draw, &g.discard, &g.exhaust, &g.limbo};
        for (const Pile *pile : piles)
            for (int i = 0; i < pile->size; ++i)
                found[(*pile)[i]] = true;
        for (int i = 0; i < g.cards.size; ++i) {
            Card& c = g.cards[i];
            if (found[i] && c.uuid == e.payload.state.ReduceCost.uuid) {
                modify_cost_for_combat(c, -1);
            }
        }
    }
};
}
