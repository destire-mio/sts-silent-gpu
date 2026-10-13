#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DamageRandomEnemy : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true}; }
    SPIRE_FN static Effect construct(const DamageInfo& info) {
        Effect e = info;
        e.kind = EffectKind::DamageRandomEnemy;
        action_initialize_state(e);
        e.target = no_target;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.target = random_living_monster(g);
        if (e.target == no_target) return;
        Effect hit = e;
        hit.kind = EffectKind::Damage;
        action_initialize_state(hit);
        add_top(g, hit);
    }
};
}
