#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SlaverRed : MonsterHooks {
    SPIRE_STATE(SlaverRed, int16_t VULN_AMT; int16_t scrapeDmg; int16_t stabDmg; bool firstTurn; bool usedEntangle;)
    SPIRE_FN static constexpr MonsterData data() { return {48, 52, {14, 9}, 0, 2, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::SlaverRed, data(), hp_override);
        m.state.SlaverRed.VULN_AMT = 1;
        m.state.SlaverRed.scrapeDmg = m.damage[1];
        m.state.SlaverRed.stabDmg = m.damage[0];
        m.state.SlaverRed.firstTurn = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (m.state.SlaverRed.firstTurn) {
            m.state.SlaverRed.firstTurn = false;
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (num >= 75 && !m.state.SlaverRed.usedEntangle) set_monster_move(g, index, 2, Intent::StrongDebuff);
        else if ((num >= 55 && m.state.SlaverRed.usedEntangle && !last_two_moves(m, 1)) || last_move(m, 3))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 3, Intent::AttackDebuff, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 2) {
            queue_power(g, index, player_target, PowerId::Entangled, 1, true);
            m.state.SlaverRed.usedEntangle = true;
        } else {
            queue_damage(g, index, player_target, m.damage[m.move == 1 ? 0 : 1]);
            if (m.move == 3) queue_power(g, index, player_target, PowerId::Vulnerable, m.state.SlaverRed.VULN_AMT + 1, true);
        }
        queue_monster_roll(g, index);
    }
};
}
