#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ChangeState : ActionHooks {
    SPIRE_STATE(ChangeState, bool called; int m; char stateName[32];)
    SPIRE_FN static Effect make(int monster, const char* state) {
        Effect e{};
        e.kind = EffectKind::ChangeState;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.ChangeState.m = monster;
        int i = 0;
        while (state[i]) {
            SPIRE_ASSERT(i < 31);
            e.state.ChangeState.stateName[i] = state[i];
            ++i;
        }
        e.state.ChangeState.stateName[i] = '\0';
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.ChangeState.called) {
            monster_changeState(g, e.state.ChangeState.m, e.state.ChangeState.stateName);
            e.state.ChangeState.called = true;
            e.done = true;
        }
    }
};
}
