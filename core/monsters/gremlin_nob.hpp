#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinNob : MonsterHooks {
    SPIRE_STATE(GremlinNob, bool usedBellow; bool canVuln;)
    SPIRE_SNAPSHOT("GremlinNob.rushDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("GremlinNob.bashDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_FN static constexpr MonsterData data() { return {85, 90, {16, 8}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 8) { d.hp_min = 82; d.hp_max = 86; }
        if (g.run.ascension < 3) { d.damage[0] = 14; d.damage[1] = 6; }
        Monster m = make_monster(g, MonsterId::GremlinNob, d, hp);
        m.state.GremlinNob.canVuln = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (!m.state.GremlinNob.usedBellow) {
            m.state.GremlinNob.usedBellow = true;
            set_monster_move(g, owner, 3, Intent::Buff);
            return;
        }
        const bool before = m.move_history.size >= 2 && g.move_histories.last(m.move_history, 1) == 2;
        const bool bash = last_two_moves(g, m, 1) || (g.run.ascension >= 18 ? !last_move(g, m, 2) && !before : num < 33);
        if (bash) set_monster_move(g, owner, 2, m.state.GremlinNob.canVuln ? Intent::AttackDebuff : Intent::Attack, monster_base_damage(g, m, 1));
        else set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 3) queue_power(g, owner, owner, PowerId::Anger, g.run.ascension >= 18 ? 3 : 2);
        else {
            queue_monster_damage(g, owner, player_target, m.move == 2 ? 1 : 0);
            if (m.move == 2 && m.state.GremlinNob.canVuln) queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
        }
        queue_monster_roll(g, owner);
    }
};
}
