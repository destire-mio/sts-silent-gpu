#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinThief : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {11, 15, {10, 0}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::GremlinThief, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        queue_damage(g, index, player_target, m.damage[0]);
        queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
};
}
