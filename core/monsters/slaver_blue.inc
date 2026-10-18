#pragma once
#include "core/monsters/base.hpp"

namespace spire::units::monsters {
struct SlaverBlue : MonsterHooks {
    SPIRE_STATE(SlaverBlue, int16_t rakeDmg; int16_t stabDmg; int16_t weakAmt;)
    SPIRE_FN static constexpr MonsterData data() { return {48, 52, {13, 8}, 0, 2, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 46; stats.hp_max = 50; }
        if (g.run.ascension < 2) { stats.damage[0] = 12; stats.damage[1] = 7; }
        Monster m = make_monster(g, MonsterId::SlaverBlue, stats, hp_override);
        m.state.SlaverBlue.rakeDmg = monster_base_damage(g, m, 1);
        m.state.SlaverBlue.stabDmg = monster_base_damage(g, m, 0);
        m.state.SlaverBlue.weakAmt = 1;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num >= 40 && !last_two_moves(g, m, 1)) || (g.run.ascension >= 17 ? last_move(g, m, 4) : last_two_moves(g, m, 4)))
            set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 4, Intent::AttackDebuff, monster_base_damage(g, m, 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        queue_monster_damage(g, index, player_target, m.move == 1 ? 0 : 1);
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, m.state.SlaverBlue.weakAmt + (g.run.ascension >= 17 ? 1 : 0), true);
        queue_monster_roll(g, index);
    }
};
}
