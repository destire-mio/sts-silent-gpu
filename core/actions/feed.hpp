#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Feed : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_SNAPSHOT("FeedAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("FeedAction.increaseHpAmount", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target == no_target)
            return;
        creature_damage(g, e);
        const Creature &c = get_creature(g, e.target);
        if ((c.dying || c.hp <= 0) && !c.half_dead && !has_power(g, c, "Minion"))
            increase_max_hp(g, e.extra);
        if (monsters_basically_dead(g))
            clear_post_combat_actions(g);
    }
};
}
