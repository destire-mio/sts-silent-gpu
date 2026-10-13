#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SpikeSlime_S : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {11, 15, {6, 0}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 10; stats.hp_max = 14; }
        if (g.run.ascension < 2) stats.damage[0] = 5;
        return make_monster(g, MonsterId::SpikeSlime_S, stats, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        queue_damage(g, index, player_target, m.damage[0]);
        queue_monster_roll(g, index);
    }
};
}
