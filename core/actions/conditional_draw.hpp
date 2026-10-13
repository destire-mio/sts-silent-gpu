#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ConditionalDrawAction : ActionHooks {
    SPIRE_SNAPSHOT("ConditionalDrawAction.restrictedType", card_type, static_cast<int>(effect.state.ConditionalDrawAction.restricted_type))
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_STATE(ConditionalDrawAction, CardType restricted_type;)
    SPIRE_FN static Effect construct(int amount, CardType restricted) {
        Effect e{};
        e.kind = EffectKind::ConditionalDrawAction;
        action_initialize_state(e);
        e.source = e.target = player_target;
        e.amount = amount;
        e.state.ConditionalDrawAction.restricted_type = restricted;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.hand.size; ++i) {
            if (card_data(g.cards[g.hand[i]].id).type == e.state.ConditionalDrawAction.restricted_type) {
                e.done = true;
                return;
            }
        }
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.source = e.source;
        draw.amount = e.amount;
        add_top(g, draw);
        e.done = true;
    }
};
}
