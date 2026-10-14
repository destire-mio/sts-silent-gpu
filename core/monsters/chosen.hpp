#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Chosen : MonsterHooks {
    SPIRE_STATE(Chosen, bool firstTurn; bool usedHex;)
    SPIRE_SNAPSHOT("Chosen.zapDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Chosen.debilitateDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_SNAPSHOT("Chosen.pokeDmg", scalar, monster_base_damage(graph.game, monster, 2))
    SPIRE_FN static constexpr MonsterData data() { return {98, 103, {21, 12, 6}, 0, 0, 3}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 95; d.hp_max = 99; }
        if (g.run.ascension < 2) { d.damage[0] = 18; d.damage[1] = 10; d.damage[2] = 5; }
        Monster m = make_monster(g, MonsterId::Chosen, d, hp);
        m.state.Chosen.firstTurn = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.Chosen;
        if (g.run.ascension < 17 && s.firstTurn) {
            s.firstTurn = false;
            set_monster_move(g, owner, 5, Intent::Attack, monster_base_damage(g, m, 2), 2);
        } else if (!s.usedHex) {
            s.usedHex = true;
            set_monster_move(g, owner, 4, Intent::StrongDebuff);
        } else if (!last_move(g, m, 3) && !last_move(g, m, 2)) {
            if (num < 50) set_monster_move(g, owner, 3, Intent::AttackDebuff, monster_base_damage(g, m, 1));
            else set_monster_move(g, owner, 2, Intent::Debuff);
        } else if (num < 40) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, owner, 5, Intent::Attack, monster_base_damage(g, m, 2), 2);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        switch (m.move) {
        case 5:
            queue_monster_damage(g, owner, player_target, 2);
            queue_monster_damage(g, owner, player_target, 2);
            break;
        case 1: queue_monster_damage(g, owner, player_target, 0); break;
        case 2:
            queue_power(g, owner, player_target, PowerId::Weak, 3, true);
            queue_power(g, owner, owner, PowerId::Strength, 3);
            break;
        case 3:
            queue_monster_damage(g, owner, player_target, 1);
            queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
            break;
        case 4: queue_power(g, owner, player_target, PowerId::Hex, 1); break;
        }
        queue_monster_roll(g, owner);
    }
};
}
