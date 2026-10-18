#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct RelicAboveCreature : ActionHooks {
    SPIRE_STATE(RelicAboveCreature, bool used;)
    SPIRE_SNAPSHOT("RelicAboveCreatureAction.relic", relic, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    SPIRE_FN static void execute(Game&, Effect& e) { e.payload.state.RelicAboveCreature.used = true; }
};
struct SFX : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
struct AnimateShake : ActionHooks {
    SPIRE_STATE(AnimateShake, bool called; float shakeDur;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::AnimateShake; action_initialize_state(e);
        e.source = owner; e.target = no_target;
        e.payload.state.AnimateShake.shakeDur = 1.0f;
        return e;
    }
    SPIRE_FN static void execute(Game&, Effect& e) { e.payload.state.AnimateShake.called = true; }
};
struct HideHealthBar : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
struct ShakeScreen : ActionHooks {
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
struct Talk : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::Talk; e.source = e.target = owner; return e;
    }
    static SPIRE_FN void execute(Game&, const Effect&) {}
};
struct Shout : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::Shout; e.source = e.target = owner; return e;
    }
    static SPIRE_FN void execute(Game&, const Effect&) {}
};
struct TextAboveCreature : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::TextAboveCreature; e.source = e.target = owner; return e;
    }
    static SPIRE_FN void execute(Game&, const Effect&) {}
};
struct AnimateFastAttack : ActionHooks {
    SPIRE_STATE(AnimateFastAttack, bool called;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::AnimateFastAttack; action_initialize_state(e); e.source = owner; e.target = no_target; return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.payload.state.AnimateFastAttack.called = true; }
};
struct AnimateSlowAttack : ActionHooks {
    SPIRE_STATE(AnimateSlowAttack, bool called;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::AnimateSlowAttack; action_initialize_state(e); e.source = owner; e.target = no_target; return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.payload.state.AnimateSlowAttack.called = true; }
};
struct AnimateHop : ActionHooks {
    SPIRE_STATE(AnimateHop, bool called;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::AnimateHop; action_initialize_state(e); e.source = owner; e.target = no_target; return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.payload.state.AnimateHop.called = true; }
};
struct AnimateJump : ActionHooks {
    SPIRE_STATE(AnimateJump, bool called;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::AnimateJump; action_initialize_state(e); e.source = owner; e.target = no_target; return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.payload.state.AnimateJump.called = true; }
};
struct SetAnimation : ActionHooks {
    SPIRE_STATE(SetAnimation, bool called;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::SetAnimation; action_initialize_state(e); e.source = owner; e.target = no_target; return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.payload.state.SetAnimation.called = true; }
};
struct FastShake : ActionHooks {
    SPIRE_STATE(FastShake, bool called;)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int owner) {
        Effect e{}; e.kind = EffectKind::FastShake; action_initialize_state(e); e.source = owner; e.target = no_target; return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.payload.state.FastShake.called = true; }
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
    SPIRE_FN static void execute(Game& g, const Effect&) {
        // AbstractMonster.updateDeathAnimation clears deaths preceding the monster's intent flash.
        uint64_t deaths = g.deaths_before_monster_turn;
        while (deaths) {
            Creature& creature = g.monsters[__builtin_ctzll(deaths)].creature;
            if (creature.dying) { creature.dead = true; creature.powers.clear(); }
            deaths &= deaths - 1;
        }
        // IntentFlashAction.update ticks ACTION_DUR_XLONG before the queued monster actions.
        advance_escape(g, 90);
    }
};
}
