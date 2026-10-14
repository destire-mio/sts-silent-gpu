#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ExhaustAllNonAttack : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN void execute(Game& g, Effect &) {
        for (int i = 0; i < g.hand.size; ++i)
            if (card_data(g.cards[g.hand[i]].id).type != CardType::Attack) {
                Effect a{};
                a.kind = EffectKind::ExhaustCard;
                action_initialize_state(a);
                a.source = a.target = no_target;
                a.card = g.hand[i];
                add_top(g, a);
            }
    }
};
}
