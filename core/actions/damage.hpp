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
    static SPIRE_FN constexpr uint32_t monster_identity(int slot) { return (uint32_t{1} << 31) | slot; }
    static SPIRE_FN DamageInfo monster_info(const Game& g, int source, int slot) {
        const Monster& monster = g.monsters[source];
        DamageInfo info{};
        info.source = source;
        info.info_base = monster_base_damage(g, monster, slot);
        info.amount = monster_output_damage(g, monster, slot);
        info.damage_type = monster_data(monster.id).damage_types[slot];
        return info;
    }
    static SPIRE_FN DamageInfo resolve_info(const Game& g, const Effect& e, uint32_t identity) {
        if ((identity & (uint32_t{1} << 31)) == 0) return e;
        return monster_info(g, e.source, identity & ~(uint32_t{1} << 31));
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
        DamageInfo info = resolve_info(g, e, e.payload.state.Damage.info_instance);
        info.target = e.target;
        damage(g, info);
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
