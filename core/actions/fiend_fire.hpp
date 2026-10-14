#pragma once
#include "../hooks.hpp"
#include "exhaust.hpp"
#include "damage.hpp"

namespace spire::units::actions {
struct FiendFire : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_SNAPSHOT("FiendFireAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        const int count = g.hand.size;
        const uint32_t identity = Damage::info_identity(g);
        for (int i = 0; i < count; ++i) {
            add_top(g, Damage::from_shared_info(e, e.target, identity));
        }
        for (int i = 0; i < count; ++i)
            add_top(g, units::actions::Exhaust::make(1, true, true));
    }
};
}
