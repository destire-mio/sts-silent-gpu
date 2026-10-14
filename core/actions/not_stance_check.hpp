#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct NotStanceCheck : ActionHooks {
    SPIRE_WAITING_RULE("NotStanceCheckAction: waiting for a ported caller with three-seed oracle replay")
    SPIRE_STATE(NotStanceCheck, int action; Stance stance;)
    SPIRE_SNAPSHOT("NotStanceCheckAction.actionToBuffer", action_reference, effect.payload.state.NotStanceCheck.action)
    SPIRE_SNAPSHOT("NotStanceCheckAction.stanceToCheck", stance_name, effect.payload.state.NotStanceCheck.stance)
    static SPIRE_FN Effect make(Game& g, Stance stance, const Effect& action) {
        Effect e{};
        e.kind = EffectKind::NotStanceCheck;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.NotStanceCheck.stance = stance;
        e.payload.state.NotStanceCheck.action = hold_action(g, action);
        return e;
    }
    static SPIRE_FN void release(Game& g, Effect& e) {
        // EmptyBody/EmptyFist/EmptyMind.use buffer only a VFXAction, which owns no payload.
        const int action = e.payload.state.NotStanceCheck.action;
        if (g.action_objects[action].held) {
            g.action_objects.release(action);
        }
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.player.stance != e.payload.state.NotStanceCheck.stance) {
            g.action_objects[e.payload.state.NotStanceCheck.action].held = false;
            g.manager.actions.push_back(e.payload.state.NotStanceCheck.action);
        }
    }
};
}
