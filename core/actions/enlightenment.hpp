#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Enlightenment : ActionHooks {
    SPIRE_STATE(Enlightenment, bool forCombat;)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.Enlightenment.forCombat = e.flag;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.hand.size; ++i) {
            Card& card = g.cards[g.hand[i]];
            if (card.cost_for_turn > 1) card.cost_for_turn = 1;
            if (e.state.Enlightenment.forCombat && card.cost > 1) card.cost = 1;
        }
    }
};
}
