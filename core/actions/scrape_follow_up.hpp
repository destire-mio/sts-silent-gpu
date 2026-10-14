#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ScrapeFollowUp : ActionHooks {
    SPIRE_WAITING_RULE("ScrapeFollowUpAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN void execute(Game& g, Effect &) {
        const Pile cards = g.drawn_cards;
        for (int i = 0; i < cards.size; ++i) {
            const int card = cards[i];
            if (g.cards[card].cost_for_turn == 0 || card_freeToPlay(g, card))
                continue;
            remove_from_pile(g.hand, card);
            move_to_discard(g, card);
            card_triggerOnManualDiscard(g, card);
            increment_discard(g, false);
        }
    }
};
}
