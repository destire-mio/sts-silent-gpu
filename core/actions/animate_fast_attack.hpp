#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AnimateFastAttack : ActionHooks {
    SPIRE_STATE(AnimateFastAttack, bool called;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.target = no_target;
        e.duration = 0.25f;
        e.start_duration = 0.0f;
    }
    SPIRE_FN static void execute(Game&, Effect& e) {
        e.state.AnimateFastAttack.called = true;
        finish_action_duration(e);
    }
};
}
