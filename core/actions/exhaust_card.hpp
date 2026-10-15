#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ExhaustCard : ActionHooks {
    SPIRE_SNAPSHOT("ExhaustSpecificCardAction.targetCard", card, effect.card)
    SPIRE_SNAPSHOT("ExhaustSpecificCardAction.group", pile_group, effect.payload.state.ExhaustCard.group)
    static SPIRE_FN constexpr ActionData data() { return {false, "EXHAUST"}; }
    SPIRE_STATE(ExhaustCard, PileId group;)
    SPIRE_FN static Effect make(int card, PileId group) {
        Effect e{};
        e.kind = EffectKind::ExhaustCard;
        action_initialize_state(e);
        e.card = card;
        e.payload.state.ExhaustCard.group = group;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const Pile& pile = get_pile(g, e.payload.state.ExhaustCard.group);
        for (int i = 0; i < pile.size; ++i) {
            if (pile[i] != e.card) continue;
            exhaust_from_pile(g, e.card, e.payload.state.ExhaustCard.group);
            g.cards[e.card].exhaust_on_use_once = false;
            g.cards[e.card].free_to_play_once = false;
            break;
        }
        // ExhaustSpecificCardAction.update always ticks ACTION_DUR_FAST, even for an absent card.
        advance_escape(g, 15);
    }
};
}
