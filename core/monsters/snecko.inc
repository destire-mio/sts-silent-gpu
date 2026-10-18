#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Snecko : MonsterHooks {
    SPIRE_STATE(Snecko, bool firstTurn;)
    SPIRE_SNAPSHOT("Snecko.biteDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Snecko.tailDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_FN static constexpr MonsterData data() { return {120, 125, {18, 10}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 114; d.hp_max = 120; }
        if (g.run.ascension < 2) { d.damage[0] = 15; d.damage[1] = 8; }
        Monster m = make_monster(g, MonsterId::Snecko, d, hp);
        m.state.Snecko.firstTurn = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (m.state.Snecko.firstTurn) {
            m.state.Snecko.firstTurn = false;
            set_monster_move(g, owner, 1, Intent::StrongDebuff);
        } else if (num < 40 || last_two_moves(g, m, 2)) set_monster_move(g, owner, 3, Intent::AttackDebuff, monster_base_damage(g, m, 1));
        else set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) {
            add_bottom(g, actions::VFX::make(owner));
            queue_power(g, owner, player_target, PowerId::Confusion, -1);
        } else if (m.move == 2) {
            add_bottom(g, actions::VFX::make());
            queue_monster_damage(g, owner, player_target, 0);
        }
        else {
            queue_monster_damage(g, owner, player_target, 1);
            if (g.run.ascension >= 17) queue_power(g, owner, player_target, PowerId::Weak, 2, true);
            queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
        }
        queue_monster_roll(g, owner);
    }
};
}
