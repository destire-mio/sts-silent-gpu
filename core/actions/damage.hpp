#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Damage : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.target == no_target) return;
        if (e.damage_type != DamageType::Thorns) {
            const Creature& target = get_creature(g, e.target);
            if (target.dying || target.half_dead || (e.target != player_target && target.escaped)) return;
            if (e.source != no_target) {
                const Creature& source = get_creature(g, e.source);
                if (source.dying || source.half_dead) return;
            }
        }
        Creature& target = get_creature(g, e.target);
        int& gold = e.target == player_target ? g.run.gold : target.gold;
        if (e.extra != 0 && gold != 0) {
            e.extra = min(e.extra, gold);
            gold -= e.extra;
            if (e.source == player_target) gain_gold(g, e.extra);
            else get_creature(g, e.source).gold += e.extra;
        }
        damage(g, e);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
