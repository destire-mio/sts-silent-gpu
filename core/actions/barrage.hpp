#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Barrage : ActionHooks {
    SPIRE_WAITING_RULE("BarrageAction: waiting for a ported caller with three-seed oracle replay")
    SPIRE_SNAPSHOT("BarrageAction.target", creature, effect.extra)
    SPIRE_SNAPSHOT("BarrageAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        for (int i = 0; i < filled_orb_count(g); ++i) {
            Effect hit = e;
            hit.kind = EffectKind::Damage;
            action_initialize_state(hit);
            hit.target = e.extra;
            hit.flag = true;
            add_top(g, hit);
        }
    }
};
}
