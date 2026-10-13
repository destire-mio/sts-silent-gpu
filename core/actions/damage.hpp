#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Damage : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.duration = 0.1f; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!alive(get_creature(g, e.target)) ||
            (e.damage_type != DamageType::Thorns && e.source != no_target && !alive(get_creature(g, e.source)))) return;
        finish_action_duration(e);
        damage(g, e);
        Effect wait{};
        wait.kind = EffectKind::Wait;
        wait.duration = 0.1f;
        add_top(g, wait);
    }
};
}
