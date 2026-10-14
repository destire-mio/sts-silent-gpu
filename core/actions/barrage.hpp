#pragma once
#include "../hooks.hpp"
#include "damage.hpp"

namespace spire::units::actions {
struct Barrage : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("BarrageAction.target", creature, effect.extra)
    SPIRE_SNAPSHOT("BarrageAction.info", damage_info, effect, queue, index)
    static SPIRE_FN Effect construct(int target, const DamageInfo& info) {
        Effect e = info;
        e.kind = EffectKind::Barrage;
        action_initialize_state(e);
        e.target = no_target;
        e.extra = target;
        return e;
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        const uint32_t identity = Damage::info_identity(g);
        for (int i = 0; i < filled_orb_count(g); ++i) {
            add_top(g, Damage::from_shared_info(e, e.extra, identity, true));
        }
    }
};
}
