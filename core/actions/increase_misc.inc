#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct IncreaseMisc : ActionHooks {
    SPIRE_STATE(IncreaseMisc, uint32_t uuid;)
    SPIRE_SNAPSHOT("IncreaseMiscAction.uuid", uuid, effect.payload.state.IncreaseMisc.uuid)
    SPIRE_SNAPSHOT("IncreaseMiscAction.miscIncrease", scalar, effect.extra)
    static SPIRE_FN void increase(Game& g, const Effect& e, int index) {
        Card& c = get_card(g, index);
        c.misc += e.extra;
        card_applyPowers(g, index);
        c.base_block_adjust += c.misc - card_baseBlock(c);
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (g.run.master_deck[i].uuid == e.payload.state.IncreaseMisc.uuid)
                increase(g, e, -i - 2);
        bool found[card_capacity]{};
        if (g.player.card_in_use >= 0)
            found[g.player.card_in_use] = true;
        const Pile *piles[] = {&g.hand, &g.draw, &g.discard, &g.exhaust, &g.limbo};
        for (const Pile *pile : piles)
            for (int i = 0; i < pile->size; ++i)
                found[(*pile)[i]] = true;
        for (int i = 0; i < g.cards.size; ++i)
            if (found[i] && g.cards[i].uuid == e.payload.state.IncreaseMisc.uuid)
                increase(g, e, i);
    }
};
}
