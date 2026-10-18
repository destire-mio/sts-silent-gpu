#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct LoseHP : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        Effect info = e;
        info.info_base = info.amount;
        info.damage_type = DamageType::HpLoss;
        damage(g, info);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
