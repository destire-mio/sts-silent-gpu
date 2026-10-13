#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct AcidSlime_M : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {29, 34, {8, 12}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::AcidSlime_M, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (num < 40) {
            if (!last_two_moves(m, 1)) set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
            else {
                const bool attack = g.rng.ai.randomBoolean();
                if (attack) set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
                else set_monster_move(g, index, 4, Intent::Debuff);
            }
        } else if (num < 80) {
            if (!last_two_moves(m, 2)) set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
            else if (g.rng.ai.randomBoolean(0.5f))
                set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
            else set_monster_move(g, index, 4, Intent::Debuff);
        } else if (!last_move(m, 4)) set_monster_move(g, index, 4, Intent::Debuff);
        else if (g.rng.ai.randomBoolean(0.4f)) set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        else {
            queue_damage(g, index, player_target, m.damage[m.move == 2 ? 1 : 0]);
            if (m.move == 1) queue_slimed(g, 1);
        }
        queue_monster_roll(g, index);
    }
};
}
