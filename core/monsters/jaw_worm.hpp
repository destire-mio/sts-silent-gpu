#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct JawWorm : MonsterHooks {
    SPIRE_STATE(JawWorm, int16_t bellowBlock; int16_t bellowStr; int16_t chompDmg; int16_t thrashBlock; int16_t thrashDmg; bool firstMove; bool hardMode;)
    SPIRE_FN static constexpr MonsterData data() { return {42, 46, {12, 7}, 9, 5, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::JawWorm, data(), hp_override);
        m.state.JawWorm.bellowBlock = m.block_amount;
        m.state.JawWorm.bellowStr = m.magic;
        m.state.JawWorm.chompDmg = m.damage[0];
        m.state.JawWorm.thrashBlock = 5;
        m.state.JawWorm.thrashDmg = m.damage[1];
        m.state.JawWorm.firstMove = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        const auto& s = g.monsters[owner].state.JawWorm;
        if (s.hardMode) {
            queue_power(g, owner, owner, PowerId::Strength, s.bellowStr);
            queue_block(g, owner, s.bellowBlock);
        }
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (m.state.JawWorm.firstMove) {
            m.state.JawWorm.firstMove = false;
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (num < 25) {
            if (!last_move(m, 1)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else if (g.rng.ai.randomBoolean(0.5625f)) set_monster_move(g, index, 2, Intent::DefendBuff);
            else set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
        } else if (num < 55) {
            if (!last_two_moves(m, 3)) set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
            else if (g.rng.ai.randomBoolean(0.357f)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else set_monster_move(g, index, 2, Intent::DefendBuff);
        } else if (!last_move(m, 2)) set_monster_move(g, index, 2, Intent::DefendBuff);
        else if (g.rng.ai.randomBoolean(0.416f)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1) queue_damage(g, index, player_target, m.damage[0]);
        else if (m.move == 2) {
            queue_power(g, index, index, PowerId::Strength, m.state.JawWorm.bellowStr);
            queue_block(g, index, m.state.JawWorm.bellowBlock);
        } else {
            queue_damage(g, index, player_target, m.damage[1]);
            queue_block(g, index, m.state.JawWorm.thrashBlock);
        }
        queue_monster_roll(g, index);
    }
};
}
