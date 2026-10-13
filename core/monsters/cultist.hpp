#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Cultist : MonsterHooks {
    SPIRE_STATE(Cultist, int16_t ritualAmount; bool firstMove;)
    SPIRE_FN static constexpr MonsterData data() { return {50, 56, {6, 0}, 0, 5}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 48; d.hp_max = 54; }
        Monster m = make_monster(g, MonsterId::Cultist, d, hp_override);
        m.state.Cultist.ritualAmount = g.run.ascension >= 2 ? 4 : 3;
        m.state.Cultist.firstMove = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        if (m.state.Cultist.firstMove) {
            m.state.Cultist.firstMove = false;
            set_monster_move(g, index, 3, Intent::Buff);
        } else set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            queue_power(g, index, index, PowerId::Ritual, m.state.Cultist.ritualAmount + (g.run.ascension >= 17 ? 1 : 0));
        } else {
            queue_damage(g, index, player_target, m.damage[0]);
        }
        queue_monster_roll(g, index);
    }
};
}
