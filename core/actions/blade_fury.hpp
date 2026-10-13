#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct BladeFuryAction : ActionHooks {
    SPIRE_STATE(BladeFuryAction, bool upgrade;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.state.BladeFuryAction.upgrade = e.flag;
        e.source = e.target = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        Effect discard{};
        discard.kind = EffectKind::Discard;
        discard.source = discard.target = player_target;
        discard.amount = g.hand.size;
        add_top(g, discard);
        e.done = true;
    }
};
}
