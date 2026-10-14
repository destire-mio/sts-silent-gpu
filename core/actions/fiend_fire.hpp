#pragma once
#include "../hooks.hpp"
#include "exhaust.hpp"

namespace spire::units::actions {
struct FiendFire : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_SNAPSHOT("FiendFireAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        const int count = g.hand.size;
        for (int i = 0; i < count; ++i) {
            Effect hit = e;
            hit.kind = EffectKind::Damage;
            action_initialize_state(hit);
            add_top(g, hit);
        }
        for (int i = 0; i < count; ++i)
            add_top(g, units::actions::Exhaust::make(1, true, true));
    }
};
}
