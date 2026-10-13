#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SpikeSlime_M : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {29, 34, {10, 0}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::SpikeSlime_M, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 30 && !last_two_moves(m, 1)) || (num >= 30 && last_move(m, 4)))
            set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 4, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Frail, m.magic, true);
        else {
            queue_damage(g, index, player_target, m.damage[0]);
            queue_slimed(g, 1);
        }
        queue_monster_roll(g, index);
    }
};
}
