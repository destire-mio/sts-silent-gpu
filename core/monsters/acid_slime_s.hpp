#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct AcidSlime_S : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {9, 13, {4, 0}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::AcidSlime_S, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        // AcidSlime_S.getMove checks for two attacks, then chooses another attack.
        if (last_two_moves(m, 1)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1) {
            queue_damage(g, index, player_target, m.damage[0]);
            set_monster_move(g, index, 2, Intent::Debuff);
        } else {
            queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        }
    }
};
}
