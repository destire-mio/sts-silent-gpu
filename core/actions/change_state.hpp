#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ChangeState : ActionHooks {
    SPIRE_STATE(ChangeState, bool called;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
        e.duration = 0.0f;
        e.start_duration = 0.0f;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        monster_changeState(g, e.card, e.extra);
        e.state.ChangeState.called = true;
        e.done = true;
    }
};
}
