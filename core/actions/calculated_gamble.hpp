#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct CalculatedGambleAction : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_STATE(CalculatedGambleAction, bool is_upgraded;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.payload.state.CalculatedGambleAction.is_upgraded = e.flag;
        e.source = no_target;
        e.target = player_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const int count = g.hand.size;
        if (e.payload.state.CalculatedGambleAction.is_upgraded || count != 0) {
            Effect draw{};
            draw.kind = EffectKind::Draw;
            draw.source = e.target;
            draw.amount = count + (e.payload.state.CalculatedGambleAction.is_upgraded ? 1 : 0);
            add_top(g, draw);
            Effect discard{};
            discard.kind = EffectKind::Discard;
            discard.source = discard.target = e.target;
            discard.amount = count;
            discard.extra = 1;
            add_top(g, discard);
        }
        e.done = true;
    }
};
}
