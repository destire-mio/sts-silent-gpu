#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Apotheosis : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect&) {
        const PileId piles[] = {PileId::Hand, PileId::Draw, PileId::Discard, PileId::Exhaust};
        for (int p = 0; p < 4; ++p) {
            const Pile& pile = get_pile(g, piles[p]);
            for (int i = 0; i < pile.size; ++i) {
                const int card = pile[i];
                if (!card_canUpgrade(g, card)) continue;
                card_upgrade(g.cards[card]);
                card_applyPowers(g, card);
            }
        }
    }
};
}
