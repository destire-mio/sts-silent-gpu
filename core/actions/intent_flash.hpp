#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SFX : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
struct VFX : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static Effect make(int source = no_target) {
        Effect e{};
        e.kind = EffectKind::VFX;
        e.source = e.target = source;
        return e;
    }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
struct IntentFlash : ActionHooks {
    SPIRE_SNAPSHOT("IntentFlashAction.m", monster, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
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
