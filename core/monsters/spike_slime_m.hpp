#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SpikeSlime_M : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {29, 34, {10, 0}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 28; stats.hp_max = 32; }
        if (g.run.ascension < 2) stats.damage[0] = 8;
        return make_monster(g, MonsterId::SpikeSlime_M, stats, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 30 && !last_two_moves(g, m, 1)) || (num >= 30 && (g.run.ascension >= 17 ? last_move(g, m, 4) : last_two_moves(g, m, 4))))
            set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 4, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Frail, data().magic, true);
        else {
            queue_monster_damage(g, index, player_target, 0);
            queue_slimed(g, 1);
        }
        queue_monster_roll(g, index);
    }
};
}
