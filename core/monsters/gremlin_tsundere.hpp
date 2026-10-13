#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinTsundere : MonsterHooks {
    SPIRE_STATE(GremlinTsundere, int16_t bashDmg; int16_t blockAmt;)
    SPIRE_FN static constexpr MonsterData data() { return {13, 17, {8, 0}, 11, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 12; d.hp_max = 15; }
        if (g.run.ascension < 2) d.damage[0] = 6;
        Monster m = make_monster(g, MonsterId::GremlinTsundere, d, hp_override);
        m.state.GremlinTsundere.bashDmg = monster_base_damage(g, m, 0);
        m.state.GremlinTsundere.blockAmt = g.run.ascension >= 17 ? 11 : g.run.ascension >= 7 ? 8 : 7;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        (void)num;
        set_monster_move(g, index, 1, Intent::Defend);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 99) {
            Effect e{}; e.kind = EffectKind::Escape; e.target = index; add_bottom(g, e);
            queue_monster_move(g, index, 99, Intent::Escape); return;
        }
        if (m.move == 1) {
            Effect e{};
            e.kind = EffectKind::BlockRandomMonster;
            e.source = index;
            e.amount = m.state.GremlinTsundere.blockAmt;
            add_bottom(g, e);
            int alive = 0;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const Creature& c = g.monsters[g.monster_order[i]].creature;
                if (!c.dying && !c.escaped) ++alive;
            }
            if (m.escape_next) queue_monster_move(g, index, 99, Intent::Escape);
            else if (alive > 1) set_monster_move(g, index, 1, Intent::Defend);
            else set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 0));
        } else {
            queue_damage(g, index, player_target, monster_base_damage(g, m, 0));
            if (m.escape_next) queue_monster_move(g, index, 99, Intent::Escape);
            else queue_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 0));
        }
    }
    SPIRE_FN static void deathReact(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.intent != Intent::Escape && !m.creature.dying) {
            set_monster_move(g, owner, 99, Intent::Escape);
            monster_createIntent(g, owner);
        }
    }
};
}
