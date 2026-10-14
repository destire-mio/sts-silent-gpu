#pragma once
#include "../hooks.hpp"
#include "change_stance.hpp"

namespace spire::units::actions {
struct FearNoEvil : ActionHooks {
    SPIRE_SNAPSHOT("FearNoEvilAction.m", creature, effect.extra)
    SPIRE_SNAPSHOT("FearNoEvilAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("AbstractGameAction.source", creature, no_target)
    SPIRE_SNAPSHOT("AbstractGameAction.target", creature, no_target)
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.extra >= 0) {
            const Intent intent = g.monsters[e.extra].intent;
            if (intent == Intent::Attack || intent == Intent::AttackBuff || intent == Intent::AttackDebuff ||
                intent == Intent::AttackDefend)
                add_top(g, units::actions::ChangeStance::make(Stance::Calm));
        }
        Effect hit = e;
        hit.kind = EffectKind::Damage;
        action_initialize_state(hit);
        hit.target = e.extra;
        add_top(g, hit);
    }
};
}
