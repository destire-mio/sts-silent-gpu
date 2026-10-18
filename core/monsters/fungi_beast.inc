#pragma once
#include "core/monsters/base.hpp"

namespace spire::units::monsters {
struct FungiBeast : MonsterHooks {
    SPIRE_STATE(FungiBeast, int16_t biteDamage; int16_t strAmt;)
    SPIRE_FN static constexpr MonsterData data() { return {24, 28, {6, 0}, 0, 5}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) d.hp_min = 22;
        Monster m = make_monster(g, MonsterId::FungiBeast, d, hp_override);
        m.state.FungiBeast.biteDamage = monster_base_damage(g, m, 0);
        m.state.FungiBeast.strAmt = g.run.ascension >= 2 ? 4 : 3;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 60 && !last_two_moves(g, m, 1)) || (num >= 60 && last_move(g, m, 2)))
            set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 2, Intent::Buff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1) {
            queue_monster_damage(g, index, player_target, 0);
        }
        else queue_power(g, index, index, PowerId::Strength, m.state.FungiBeast.strAmt + (g.run.ascension >= 17 ? 1 : 0));
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::SporeCloud, 2);
    }
};
}
