#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct VampireDamageAction : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info) {
        Effect e{};
        e.kind = EffectKind::VampireDamageAction;
        action_initialize_state(e);
        e.target = target;
        e.source = info.source;
        e.amount = info.amount;
        e.info_base = info.info_base;
        e.damage_type = info.damage_type;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.target == player_target) {
            player_damage(g, e);
        } else {
            monster_damage(g, e.target, e);
            if (monsters_basically_dead(g)) {
                g.battle_over = true;
                g.manager.card_queue.clear();
            }
        }
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
        e.done = true;
    }
};
}
