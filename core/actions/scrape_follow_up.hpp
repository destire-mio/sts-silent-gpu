#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ScrapeFollowUp : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect &) {
        const Pile cards = g.drawn_cards;
        for (int i = 0; i < cards.size; ++i) {
            const int card = cards[i];
            if (g.cards[card].cost_for_turn == 0 || g.cards[card].free_to_play_once)
                continue;
            remove_from_pile(g.hand, card);
            move_to_discard(g, card);
            card_triggerOnManualDiscard(g, card);
            increment_discard(g, false);
        }
    }
};
}
