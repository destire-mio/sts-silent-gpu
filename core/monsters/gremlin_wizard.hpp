#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinWizard : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {22, 26, {30, 0}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::GremlinWizard, data(), hp_override);
        m.turn_count = 1;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        (void)num;
        set_monster_move(g, index, 2, Intent::Unknown);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 2) {
            ++m.turn_count;
            if (m.turn_count == 3) queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else set_monster_move(g, index, 2, Intent::Unknown);
        } else {
            m.turn_count = 0;
            queue_damage(g, index, player_target, m.damage[0]);
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        }
    }
};
}
