#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SnakePlant : MonsterHooks {
    SPIRE_SNAPSHOT("SnakePlant.rainBlowsDmg", scalar, monster.damage[0])
    SPIRE_FN static constexpr MonsterData data() { return {78, 82, {8}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 75; d.hp_max = 79; }
        if (g.run.ascension < 2) d.damage[0] = 7;
        Monster m = make_monster(g, MonsterId::SnakePlant, d, hp);
        position_monster(m, -30.0f);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::Malleable, 3); }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        const bool before = m.move_history.size >= 2 && m.move_history[m.move_history.size - 2] == 2;
        const bool spores = num < 65 ? last_two_moves(m, 1) : !(last_move(m, 2) || (g.run.ascension >= 17 && before));
        if (spores) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else set_monster_move(g, owner, 1, Intent::Attack, m.damage[0], 3);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) for (int i = 0; i < 3; ++i) queue_damage(g, owner, player_target, m.damage[0]);
        else {
            queue_power(g, owner, player_target, PowerId::Frail, 2, true);
            queue_power(g, owner, player_target, PowerId::Weak, 2, true);
        }
        queue_monster_roll(g, owner);
    }
};
}
