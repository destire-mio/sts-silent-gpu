#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct JawWorm : MonsterHooks {
    SPIRE_SNAPSHOT("JawWorm.bellowBlock", scalar, monster.block_amount)
    SPIRE_SNAPSHOT("JawWorm.bellowStr", scalar, monster.magic)
    SPIRE_SNAPSHOT("JawWorm.chompDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("JawWorm.thrashBlock", scalar, 5)
    SPIRE_SNAPSHOT("JawWorm.thrashDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_STATE(JawWorm, bool firstMove; bool hardMode;)
    SPIRE_FN static constexpr MonsterData data() { return {42, 46, {12, 7}, 9, 5, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 40; stats.hp_max = 44; }
        if (g.run.ascension < 17) { stats.block = 6; stats.magic = g.run.ascension >= 2 ? 4 : 3; }
        if (g.run.ascension < 2) stats.damage[0] = 11;
        Monster m = make_monster(g, MonsterId::JawWorm, stats, hp_override);
        m.state.JawWorm.firstMove = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        const auto& s = m.state.JawWorm;
        if (s.hardMode) {
            queue_power(g, owner, owner, PowerId::Strength, m.magic);
            queue_block(g, owner, m.block_amount);
        }
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (m.state.JawWorm.firstMove) {
            m.state.JawWorm.firstMove = false;
            set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        } else if (num < 25) {
            if (!last_move(g, m, 1)) set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
            else if (g.rng.ai.randomBoolean(0.5625f)) set_monster_move(g, index, 2, Intent::DefendBuff);
            else set_monster_move(g, index, 3, Intent::AttackDefend, monster_base_damage(g, m, 1));
        } else if (num < 55) {
            if (!last_two_moves(g, m, 3)) set_monster_move(g, index, 3, Intent::AttackDefend, monster_base_damage(g, m, 1));
            else if (g.rng.ai.randomBoolean(0.357f)) set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
            else set_monster_move(g, index, 2, Intent::DefendBuff);
        } else if (!last_move(g, m, 2)) set_monster_move(g, index, 2, Intent::DefendBuff);
        else if (g.rng.ai.randomBoolean(0.416f)) set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 3, Intent::AttackDefend, monster_base_damage(g, m, 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1) queue_damage(g, index, player_target, monster_base_damage(g, m, 0));
        else if (m.move == 2) {
            queue_power(g, index, index, PowerId::Strength, m.magic);
            queue_block(g, index, m.block_amount);
        } else {
            queue_damage(g, index, player_target, monster_base_damage(g, m, 1));
            queue_block(g, index, 5);
        }
        queue_monster_roll(g, index);
    }
};
}
