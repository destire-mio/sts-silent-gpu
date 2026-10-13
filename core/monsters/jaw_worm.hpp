#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct JawWorm : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {42, 46, {12, 7}, 9, 5}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::JawWorm, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (m.first_move) {
            m.first_move = false;
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (num < 25) {
            if (!last_move(m, 1)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else if (g.rng.ai.randomBoolean(0.5625f)) set_monster_move(g, index, 2, Intent::DefendBuff);
            else set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
        } else if (num < 55) {
            if (!last_two_moves(m, 3)) set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
            else if (g.rng.ai.randomBoolean(0.357f)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else set_monster_move(g, index, 2, Intent::DefendBuff);
        } else if (!last_move(m, 2)) set_monster_move(g, index, 2, Intent::DefendBuff);
        else if (g.rng.ai.randomBoolean(0.416f)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1) queue_damage(g, index, player_target, m.damage[0]);
        else if (m.move == 2) {
            queue_power(g, index, index, PowerId::Strength, m.magic);
            queue_block(g, index, m.block_amount);
        } else {
            queue_damage(g, index, player_target, m.damage[1]);
            queue_block(g, index, 5);
        }
        queue_monster_roll(g, index);
    }
};
}
