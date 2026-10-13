#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Draw : ActionHooks {
    SPIRE_SNAPSHOT("DrawCardAction.clearDrawHistory", scalar, !effect.flag)
    SPIRE_SNAPSHOT("DrawCardAction.followUpAction", action_reference, effect.payload.state.Draw.follow_up_action - 1)
    static SPIRE_FN constexpr ActionData data() { return {false, "DRAW", true}; }
    SPIRE_STATE(Draw, bool shuffle_check; int follow_up_action;)
    SPIRE_FN static Effect construct(int amount, bool clear_history = true, int follow_up = -1, int source = no_target, bool end_turn_draw = false) {
        Effect e{};
        e.kind = EffectKind::Draw;
        action_initialize_state(e);
        e.amount = amount;
        e.end_turn_draw = end_turn_draw;
        e.source = source;
        e.flag = !clear_history;
        e.extra = follow_up + 1;
        e.payload.state.Draw.follow_up_action = e.extra;
        return e;
    }
    SPIRE_FN static Effect construct(Game& g, int amount, const Effect& follow_up, bool clear_history = true) {
        return construct(amount, clear_history, hold_action(g, follow_up));
    }
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        action_initialize_state(e);
        e.payload.state.Draw.follow_up_action = e.extra;
        if (e.end_turn_draw) g.player_turn_effect_pending = true;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) { draw_cards(g, e); }
};
}
