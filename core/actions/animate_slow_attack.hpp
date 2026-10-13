#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AnimateSlowAttack : ActionHooks {
    SPIRE_STATE(AnimateSlowAttack, bool called;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.target = no_target;
        e.duration = 0.5f;
        e.start_duration = 0.5f;
    }
    SPIRE_FN static void execute(Game&, Effect& e) {
        e.state.AnimateSlowAttack.called = true;
        finish_action_duration(e);
    }
};
}
