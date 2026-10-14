#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Damage : ActionHooks {
    SPIRE_STATE(Damage, uint32_t info_instance;)
    SPIRE_SNAPSHOT("DamageAction.info", damage_info, effect, queue, index, effect.payload.state.Damage.info_instance)
    SPIRE_SNAPSHOT("DamageAction.goldAmount", scalar, effect.extra)
    SPIRE_SNAPSHOT("DamageAction.muteSfx", scalar, false)
    SPIRE_SNAPSHOT("DamageAction.skipWait", scalar, effect.flag)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN uint32_t info_identity(Game& g) {
        SPIRE_ASSERT(g.next_damage_info_instance < (uint32_t{1} << 31));
        return g.next_damage_info_instance++;
    }
    static SPIRE_FN Effect from_shared_info(const DamageInfo& info, int target, uint32_t identity, bool skip_wait = false) {
        Effect e = info;
        e.kind = EffectKind::Damage;
        action_initialize_state(e);
        e.payload.state.Damage.info_instance = identity;
        e.target = target;
        e.extra = 0;
        e.flag = skip_wait;
        return e;
    }
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
