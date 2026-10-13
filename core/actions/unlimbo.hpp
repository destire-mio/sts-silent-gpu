#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Unlimbo : ActionHooks {
    SPIRE_SNAPSHOT("UnlimboAction.card", card, effect.card)
    SPIRE_STATE(Unlimbo, bool exhaust;)
    SPIRE_FN static Effect construct(int card, bool exhaust = false) {
        Effect e{};
        e.kind = EffectKind::Unlimbo;
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
        e.card = card;
        e.payload.state.Unlimbo.exhaust = exhaust;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) { remove_from_pile(g.limbo, e.card); }
};
}
