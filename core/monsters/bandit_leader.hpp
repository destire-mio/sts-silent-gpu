#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct BanditLeader : MonsterHooks {
    SPIRE_SNAPSHOT("BanditLeader.slashDmg", scalar, monster.damage[0])
    SPIRE_SNAPSHOT("BanditLeader.agonizeDmg", scalar, monster.damage[1])
    SPIRE_SNAPSHOT("BanditLeader.weakAmount", scalar, game.run.ascension >= 17 ? 3 : 2)
    SPIRE_FN static constexpr MonsterData data() { return {37, 41, {17, 12}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 35; stats.hp_max = 39; }
        if (g.run.ascension < 2) { stats.damage[0] = 15; stats.damage[1] = 10; }
        return make_monster(g, MonsterId::BanditLeader, stats, hp_override);
    }
    SPIRE_FN static void deathReact(Game& g, int owner) {
        const Creature& c = g.monsters[owner].creature;
        if (!c.dying && !c.half_dead && !c.escaped) add_bottom(g, actions::Talk::make(owner));
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) { set_monster_move(g, owner, 2, Intent::Unknown); }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 2) queue_monster_move(g, owner, 3, Intent::AttackDebuff, m.damage[1]);
        else {
            add_bottom(g, actions::ChangeState::make(g, owner, "STAB"));
            queue_damage(g, owner, player_target, m.damage[m.move == 1 ? 0 : 1]);
            if (m.move == 3) queue_power(g, owner, player_target, PowerId::Weak, g.run.ascension >= 17 ? 3 : 2, true);
            if (m.move == 3 || (g.run.ascension >= 17 && !last_two_moves(g, m, 1)))
                queue_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
            else queue_monster_move(g, owner, 3, Intent::AttackDebuff, m.damage[1]);
        }
    }
};
}
