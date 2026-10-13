#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct IntentFlash : ActionHooks {
    SPIRE_FN static Effect make(int monster) {
        Effect e{};
        e.kind = EffectKind::IntentFlash;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.extra = monster;
        return e;
    }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
}
