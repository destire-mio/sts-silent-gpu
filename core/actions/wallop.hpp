#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Wallop : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_SNAPSHOT("WallopAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target == no_target || get_creature(g, e.target).dying || get_creature(g, e.target).half_dead ||
            (e.target != player_target && get_creature(g, e.target).escaped) ||
            (e.source != no_target && get_creature(g, e.source).dying))
            return;
        creature_damage(g, e);
        if (get_creature(g, e.target).last_damage_taken > 0) {
            Effect a{};
            a.kind = EffectKind::Block;
            a.target = e.source;
            a.source = no_target;
            a.amount = get_creature(g, e.target).last_damage_taken;
            add_top(g, a);
            a = {};
            a.kind = EffectKind::VFX;
            a.source = a.target = no_target;
            add_top(g, a);
        }
        if (monsters_basically_dead(g))
            clear_post_combat_actions(g);
    }
};
}
