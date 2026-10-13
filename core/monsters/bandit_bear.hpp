#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct BanditBear : MonsterHooks {
    SPIRE_STATE(BanditBear, int maulDmg; int lungeDmg; int con_reduction;)
    SPIRE_FN static constexpr MonsterData data() { return {40, 44, {20, 10}, 9, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 38; stats.hp_max = 42; }
        if (g.run.ascension < 2) { stats.damage[0] = 18; stats.damage[1] = 9; }
        Monster m = make_monster(g, MonsterId::BanditBear, stats, hp_override);
        m.state.BanditBear.maulDmg = m.damage[0]; m.state.BanditBear.lungeDmg = m.damage[1];
        m.state.BanditBear.con_reduction = g.run.ascension >= 17 ? -4 : -2;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) { set_monster_move(g, owner, 2, Intent::StrongDebuff); }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 1) add_bottom(g, actions::ChangeState::make(g, owner, "MAUL"));
        else add_bottom(g, actions::AnimateSlowAttack::make(owner));
        if (m.move == 2) queue_power(g, owner, player_target, PowerId::Dexterity, m.state.BanditBear.con_reduction);
        else queue_damage(g, owner, player_target, m.damage[m.move == 1 ? 0 : 1]);
        if (m.move == 3) {
            queue_block(g, owner, 9); queue_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
        } else queue_monster_move(g, owner, 3, Intent::AttackDefend, m.damage[1]);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics) {
        monster_die_base(g, owner, trigger_relics);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            const Creature& c = g.monsters[target].creature;
            if (!c.dead && !c.dying) monster_deathReact(g, target);
        }
    }
};
}
