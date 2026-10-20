#pragma once
#include "core/hooks.hpp"
#include "core/actions/wait.hpp"

namespace spire::units::actions {
struct LoseHP : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        // LoseHPAction.update deals damage after 0.33s, then queues WaitAction(0.1f).
        settle_deaths_after_wait(g, 20);
        Effect info = e;
        info.info_base = info.amount;
        info.damage_type = DamageType::HpLoss;
        damage(g, info);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
        add_top(g, Wait::make(7));
    }
};
}
