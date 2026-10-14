#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SphericGuardian : MonsterHooks {
    SPIRE_STATE(SphericGuardian, bool firstMove; bool secondMove;)
    SPIRE_SNAPSHOT("SphericGuardian.dmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_FN static constexpr MonsterData data() { return {20, 20, {11}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 2) d.damage[0] = 10;
        Monster m = make_monster(g, MonsterId::SphericGuardian, d, hp ? hp : 20);
        m.state.SphericGuardian.firstMove = m.state.SphericGuardian.secondMove = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Barricade, -1);
        queue_power(g, owner, owner, PowerId::Artifact, 3);
        queue_block(g, owner, 40);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        (void)num;
        Monster& m = g.monsters[owner];
        auto& s = m.state.SphericGuardian;
        if (s.firstMove) { s.firstMove = false; set_monster_move(g, owner, 2, Intent::Defend); }
        else if (s.secondMove) { s.secondMove = false; set_monster_move(g, owner, 4, Intent::AttackDebuff, monster_base_damage(g, m, 0)); }
        else if (last_move(g, m, 1)) set_monster_move(g, owner, 3, Intent::AttackDefend, monster_base_damage(g, m, 0));
        else set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0), 2);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        switch (m.move) {
        case 1:
            queue_monster_damage(g, owner, player_target, 0);
            queue_monster_damage(g, owner, player_target, 0);
            break;
        case 2: queue_block(g, owner, g.run.ascension >= 17 ? 35 : 25); break;
        case 3:
            queue_block(g, owner, 15);
            queue_monster_damage(g, owner, player_target, 0);
            break;
        case 4:
            queue_monster_damage(g, owner, player_target, 0);
            queue_power(g, owner, player_target, PowerId::Frail, 5, true);
            break;
        }
        queue_monster_roll(g, owner);
    }
};
}
