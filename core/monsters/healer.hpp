#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Healer : MonsterHooks {
    SPIRE_STATE(Healer, int strAmt; int healAmt;)
    SPIRE_SNAPSHOT("Healer.magicDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_FN static constexpr MonsterData data() { return {50, 58, {9}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 48; d.hp_max = 56; }
        if (g.run.ascension < 2) d.damage[0] = 8;
        Monster m = make_monster(g, MonsterId::Healer, d, hp);
        m.state.Healer.strAmt = g.run.ascension >= 17 ? 4 : g.run.ascension >= 2 ? 3 : 2;
        m.state.Healer.healAmt = g.run.ascension >= 17 ? 20 : 16;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        int missing = 0;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const Creature& c = g.monsters[g.monster_order[i]].creature;
            if (!c.dying && !c.escaped) missing += c.max_hp - c.hp;
        }
        if (missing > (g.run.ascension >= 17 ? 20 : 15) && !last_two_moves(g, m, 2)) set_monster_move(g, owner, 2, Intent::Buff);
        else if (num >= 40 && !(g.run.ascension >= 17 ? last_move(g, m, 1) : last_two_moves(g, m, 1))) set_monster_move(g, owner, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
        else if (!last_two_moves(g, m, 3)) set_monster_move(g, owner, 3, Intent::Buff);
        else set_monster_move(g, owner, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) {
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 0));
            queue_power(g, owner, player_target, PowerId::Frail, 2, true);
        } else for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            const Creature& c = g.monsters[target].creature;
            if (c.dying || c.escaped) continue;
            if (m.move == 3) queue_power(g, owner, target, PowerId::Strength, m.state.Healer.strAmt);
            else {
                Effect e{}; e.kind = EffectKind::Heal; e.source = owner; e.target = target; e.amount = m.state.Healer.healAmt; add_bottom(g, e);
            }
        }
        queue_monster_roll(g, owner);
    }
};
}
