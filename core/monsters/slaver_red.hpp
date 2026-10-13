#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SlaverRed : MonsterHooks {
    SPIRE_STATE(SlaverRed, int16_t VULN_AMT; int16_t scrapeDmg; int16_t stabDmg; bool firstTurn; bool usedEntangle;)
    SPIRE_FN static constexpr MonsterData data() { return {48, 52, {14, 9}, 0, 2, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 46; stats.hp_max = 50; }
        if (g.run.ascension < 2) { stats.damage[0] = 13; stats.damage[1] = 8; }
        Monster m = make_monster(g, MonsterId::SlaverRed, stats, hp_override);
        m.state.SlaverRed.VULN_AMT = 1;
        m.state.SlaverRed.scrapeDmg = monster_base_damage(g, m, 1);
        m.state.SlaverRed.stabDmg = monster_base_damage(g, m, 0);
        m.state.SlaverRed.firstTurn = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (m.state.SlaverRed.firstTurn) {
            m.state.SlaverRed.firstTurn = false;
            set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        } else if (num >= 75 && !m.state.SlaverRed.usedEntangle) set_monster_move(g, index, 2, Intent::StrongDebuff);
        else if ((num >= 55 && m.state.SlaverRed.usedEntangle && !last_two_moves(g, m, 1)) || (g.run.ascension >= 17 ? last_move(g, m, 3) : last_two_moves(g, m, 3)))
            set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 3, Intent::AttackDebuff, monster_base_damage(g, m, 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 2) {
            queue_power(g, index, player_target, PowerId::Entangled, 1, true);
            m.state.SlaverRed.usedEntangle = true;
        } else {
            queue_damage(g, index, player_target, monster_base_damage(g, m, m.move == 1 ? 0 : 1));
            if (m.move == 3) queue_power(g, index, player_target, PowerId::Vulnerable, m.state.SlaverRed.VULN_AMT + (g.run.ascension >= 17 ? 1 : 0), true);
        }
        queue_monster_roll(g, index);
    }
};
}
