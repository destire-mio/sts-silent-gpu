#pragma once
#include "base.hpp"
#include "../actions/summon_gremlin.hpp"
#include "../actions/apply_power.hpp"

namespace spire::units::monsters {
struct GremlinLeader : MonsterHooks {
    SPIRE_STATE(GremlinLeader, int8_t gremlins[3];)
    SPIRE_SNAPSHOT("GremlinLeader.strAmt", scalar, game.run.ascension >= 18 ? 5 : game.run.ascension >= 3 ? 4 : 3)
    SPIRE_SNAPSHOT("GremlinLeader.blockAmt", scalar, game.run.ascension >= 18 ? 10 : 6)
    SPIRE_SNAPSHOT("GremlinLeader.gremlins", creature_array, monster.state.GremlinLeader.gremlins, 3)
    SPIRE_SNAPSHOT("GremlinLeader.STAB_DMG", scalar, 6)
    SPIRE_SNAPSHOT("GremlinLeader.STAB_AMT", scalar, 3)
    SPIRE_FN static constexpr MonsterData data() { return {145, 155, {6}, 10, 5, 1, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 8) { d.hp_min = 140; d.hp_max = 148; }
        Monster m = make_monster(g, MonsterId::GremlinLeader, d, hp);
        auto& s = m.state.GremlinLeader;
        for (int i = 0; i < 3; ++i) s.gremlins[i] = no_target;

        position_monster(m, 35.0f);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        auto& s = g.monsters[owner].state.GremlinLeader;
        s.gremlins[0] = g.monster_order[0]; s.gremlins[1] = g.monster_order[1]; s.gremlins[2] = no_target;
        // GremlinLeader.usePreBattleAction makes the leader the owner of all three MinionPower objects.
        for (int i = 0; i < 3; ++i) {
            Power power{}; power.id = PowerId::Minion;
            power_constructPower(g, power, owner, owner, 0, -1);
            add_bottom(g, actions::ApplyPower::make(s.gremlins[i], s.gremlins[i], power, -1));
        }
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        int count = 0;
        for (int i = 0; i < g.monster_order.size; ++i) if (g.monster_order[i] != owner && !g.monsters[g.monster_order[i]].creature.dying) ++count;
        for (;;) {
            int move;
            if (count == 0) move = num < 75 ? (last_move(g, m, 2) ? 4 : 2) : (last_move(g, m, 4) ? 2 : 4);
            else if (count < 2) {
                if (num < 50) {
                    if (last_move(g, m, 2)) { num = g.rng.ai.random(50, 99); continue; }
                    move = 2;
                } else if (num < 80) move = last_move(g, m, 3) ? 4 : 3;
                else {
                    if (last_move(g, m, 4)) { num = g.rng.ai.random(0, 80); continue; }
                    move = 4;
                }
            } else move = num < 66 ? (last_move(g, m, 3) ? 4 : 3) : (last_move(g, m, 4) ? 3 : 4);
            if (move == 2) set_monster_move(g, owner, 2, Intent::Unknown);
            else if (move == 3) set_monster_move(g, owner, 3, Intent::DefendBuff);
            else set_monster_move(g, owner, 4, Intent::Attack, 6, 3);
            return;
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.GremlinLeader;
        if (m.move == 2) {
            add_bottom(g, actions::SummonGremlinAction::construct(g, s.gremlins));
            add_bottom(g, actions::SummonGremlinAction::construct(g, s.gremlins));
        } else if (m.move == 3) {
            g.rng.ai.random(0, 2);
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int target = g.monster_order[i];
                if (target != owner && g.monsters[target].creature.dying) continue;
                queue_power(g, owner, target, PowerId::Strength, (g.run.ascension >= 18 ? 5 : g.run.ascension >= 3 ? 4 : 3));
                if (target != owner) queue_block(g, target, (g.run.ascension >= 18 ? 10 : 6));
            }
        } else for (int i = 0; i < 3; ++i) queue_damage(g, owner, player_target, m.damage[0]);
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            if (g.monsters[target].creature.dying) continue;
            Effect e{}; e.kind = EffectKind::Escape; e.target = target; add_bottom(g, e);
        }
    }
};
}
