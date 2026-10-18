#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage.hpp"

namespace spire::units::actions {
struct VampireDamageAction : ActionHooks {
    SPIRE_STATE(VampireDamageAction, uint32_t info_instance;)
    SPIRE_SNAPSHOT("VampireDamageAction.info", damage_info, effect, queue, index, effect.payload.state.VampireDamageAction.info_instance)
    SPIRE_FN static constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info, uint32_t identity = 0) {
        Effect e{};
        e.kind = EffectKind::VampireDamageAction;
        action_initialize_state(e);
        e.payload.state.VampireDamageAction.info_instance = identity;
        e.target = target;
        e.source = info.source;
        e.amount = info.amount;
        e.info_base = info.info_base;
        e.damage_type = info.damage_type;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        DamageInfo info = Damage::resolve_info(g, e, e.payload.state.VampireDamageAction.info_instance);
        if (e.target == player_target) {
            player_damage(g, info);
        } else {
            monster_damage(g, e.target, info);
            if (monsters_basically_dead(g)) {
                g.manager.card_queue.clear();
            }
        }
        if (get_creature(g, e.target).last_damage_taken > 0) {
            Effect heal{};
            heal.kind = EffectKind::Heal;
            heal.source = heal.target = e.source;
            heal.amount = get_creature(g, e.target).last_damage_taken;
            add_top(g, heal);
        }
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
        e.done = true;
    }
};
}
