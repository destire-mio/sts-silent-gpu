#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Cultist : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {50, 56, {6, 0}, 0, 5}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::Cultist, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        if (m.first_move) {
            m.first_move = false;
            set_monster_move(g, index, 3, Intent::Buff);
        } else set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) queue_power(g, index, index, PowerId::Ritual, m.magic);
        else queue_damage(g, index, player_target, m.damage[0]);
        queue_monster_roll(g, index);
    }
};
}
