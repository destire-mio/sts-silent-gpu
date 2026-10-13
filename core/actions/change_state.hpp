#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ChangeState : ActionHooks {
    SPIRE_SNAPSHOT("ChangeStateAction.m", monster, effect.state.ChangeState.m)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_SNAPSHOT("ChangeStateAction.stateName", scalar, game.text[effect.state.ChangeState.name])
    SPIRE_STATE(ChangeState, bool called; int8_t m; uint16_t name;)
    SPIRE_FN static Effect make(Game& g, int monster, const char* state) {
        Effect e{};
        e.kind = EffectKind::ChangeState;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.ChangeState.m = monster;
        e.state.ChangeState.name = g.text.intern(state);
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.ChangeState.called) {
            monster_changeState(g, e.state.ChangeState.m, g.text[e.state.ChangeState.name]);
            e.state.ChangeState.called = true;
            e.done = true;
        }
    }
};
}
