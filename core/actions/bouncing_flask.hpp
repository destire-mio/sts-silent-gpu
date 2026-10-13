#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct BouncingFlask : ActionHooks {
    SPIRE_STATE(BouncingFlask, int num_times; int amount;)
    SPIRE_FN static Effect construct(int target, int amount, int num_times) {
        Effect e{};
        e.kind = EffectKind::BouncingFlask;
        action_initialize_state(e);
        e.source = no_target;
        e.target = target;
        e.state.BouncingFlask.amount = amount;
        e.state.BouncingFlask.num_times = num_times;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.target == no_target) return;
        if (monsters_basically_dead(g)) { clear_post_combat_actions(g); return; }
        if (e.state.BouncingFlask.num_times > 1) {
            --e.state.BouncingFlask.num_times;
            const int target = random_living_monster(g);
            add_top(g, construct(target, e.state.BouncingFlask.amount, e.state.BouncingFlask.num_times));
        }
        if (get_creature(g, e.target).hp > 0) {
            Effect apply{};
            apply.kind = EffectKind::ApplyPower;
            apply.source = player_target;
            apply.target = e.target;
            apply.power = power_id("Poison");
            apply.amount = e.state.BouncingFlask.amount;
            add_top(g, apply);
        }
    }
};
}
