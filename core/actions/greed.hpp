#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Greed : ActionHooks {
    SPIRE_SNAPSHOT("GreedAction.info", damage_info, effect, queue, index)
    SPIRE_STATE(Greed, int increaseGold;)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.payload.state.Greed.increaseGold = e.extra;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.target == no_target) return;
        creature_damage(g, e);
        const Creature& target = get_creature(g, e.target);
        if ((target.dying || target.hp <= 0) && !target.half_dead && !has_power(g, target, "Minion"))
            gain_gold(g, e.payload.state.Greed.increaseGold);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
