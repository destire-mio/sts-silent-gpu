#pragma once
#include "core/hooks.hpp"
#include "core/actions/discard_specific_card.hpp"

namespace spire::units::actions {
struct UnloadAction : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.hand.size; ++i)
            if (card_data(g.cards[g.hand[i]].id).type != CardType::Attack)
                add_top(g, DiscardSpecificCard::make(g.hand[i]));
        e.done = true;
    }
};
}
