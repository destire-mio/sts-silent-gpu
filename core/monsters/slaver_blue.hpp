#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SlaverBlue : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {48, 52, {13, 8}, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::SlaverBlue, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num >= 40 && !last_two_moves(m, 1)) || last_move(m, 4))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 4, Intent::AttackDebuff, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        queue_damage(g, index, player_target, m.damage[m.move == 1 ? 0 : 1]);
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        queue_monster_roll(g, index);
    }
};
}
