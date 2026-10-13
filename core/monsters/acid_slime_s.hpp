#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct AcidSlime_S : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {9, 13, {4, 0}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 8; d.hp_max = 12; }
        if (g.run.ascension < 2) d.damage[0] = 3;
        return make_monster(g, MonsterId::AcidSlime_S, d, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        // AcidSlime_S.getMove checks for two attacks, then chooses another attack.
        if (g.run.ascension >= 17 ? last_two_moves(g, m, 1) : g.rng.ai.randomBoolean()) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
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
