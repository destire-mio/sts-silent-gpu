#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct BurnIncrease : ActionHooks {
    SPIRE_STATE(BurnIncrease, bool gotBurned;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.discard.size; ++i)
            if (g.cards[g.discard[i]].id == CardId::Burn) card_upgrade(g.cards[g.discard[i]]);
        for (int i = 0; i < g.draw.size; ++i)
            if (g.cards[g.draw[i]].id == CardId::Burn) card_upgrade(g.cards[g.draw[i]]);
        e.payload.state.BurnIncrease.gotBurned = true;
        for (int i = 0; i < 3; ++i) {
            Card burn = make_card(CardId::Burn);
            card_upgrade(burn);
            g.discard.push_back(create_card(g, burn));
        }
    }
};
}
