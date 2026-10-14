#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Gash : ActionHooks {
    SPIRE_SNAPSHOT("GashAction.card", card, effect.card)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        g.cards[e.card].base_damage += e.amount;
        card_applyPowers(g, e.card);
        const Pile *piles[] = {&g.discard, &g.draw, &g.hand};
        for (const Pile *pile : piles)
            for (int i = 0; i < pile->size; ++i) {
                const int card = (*pile)[i];
                if (!card_is(g.cards[card].id, "Gash"))
                    continue;
                g.cards[card].base_damage += e.amount;
                card_applyPowers(g, card);
            }
    }
};
}
