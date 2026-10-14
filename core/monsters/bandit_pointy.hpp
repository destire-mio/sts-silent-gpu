#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct BanditPointy : MonsterHooks {
    SPIRE_STATE(BanditPointy, int attackDmg;)
    SPIRE_FN static constexpr MonsterData data() { return {34, 34, {6}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) stats.hp_min = stats.hp_max = 30;
        if (g.run.ascension < 2) stats.damage[0] = 5;
        Monster m = make_monster(g, MonsterId::BanditPointy, stats, hp_override);
        m.state.BanditPointy.attackDmg = monster_base_damage(g, m, 0);
        return m;
    }
    SPIRE_FN static void deathReact(Game& g, int owner) {
        const Creature& c = g.monsters[owner].creature;
        if (!c.dead && !c.dying && !c.escaped) add_bottom(g, actions::Talk::make(owner));
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) { set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, g.monsters[owner], 0), 2); }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        add_bottom(g, actions::ChangeState::make(g, owner, "SLASH"));
        for (int i = 0; i < 2; ++i) queue_monster_damage(g, owner, player_target, 0);
        queue_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, g.monsters[owner], 0), 2);
    }
};
}
