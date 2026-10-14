#pragma once
#include "../hooks.hpp"
#include "attack_damage_random_enemy.hpp"

namespace spire::units::actions {
struct NewRipAndTear : ActionHooks {
    SPIRE_SNAPSHOT("AttackDamageRandomEnemyAction.card", card, effect.card)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        AttackDamageRandomEnemy::execute(g, e);
        if (e.target != no_target) {
            Effect v{};
            v.source = v.target = no_target;
            v.kind = EffectKind::VFX;
            add_top(g, v);
        }
    }
};
}
