#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DamagePerAttackPlayed : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info) {
        Effect e = info;
        e.kind = EffectKind::DamagePerAttackPlayed;
        action_initialize_state(e);
        e.target = target;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.target == no_target || get_creature(g, e.target).hp <= 0) return;
        const int count = static_cast<int>(g.manager.played_cards.turn_types[static_cast<int>(CardType::Attack)]) - 1;
        for (int i = 0; i < count; ++i) {
            Effect hit = e;
            hit.kind = EffectKind::Damage;
            action_initialize_state(hit);
            add_top(g, hit);
        }
    }
};
}
