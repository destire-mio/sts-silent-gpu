#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct PummelDamage : ActionHooks {
    SPIRE_WAITING_RULE("PummelDamageAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_SNAPSHOT("PummelDamageAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target == no_target || get_creature(g, e.target).hp <= 0)
            return;
        if (e.damage_type != DamageType::Thorns && e.source != no_target && get_creature(g, e.source).dying)
            return;
        creature_damage(g, e);
        if (monsters_basically_dead(g))
            clear_post_combat_actions(g);
    }
};
}
