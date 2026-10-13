#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinTsundere : MonsterHooks {
    SPIRE_STATE(GremlinTsundere, int16_t bashDmg; int16_t blockAmt;)
    SPIRE_FN static constexpr MonsterData data() { return {13, 17, {8, 0}, 11, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::GremlinTsundere, data(), hp_override);
        m.state.GremlinTsundere.bashDmg = m.damage[0];
        m.state.GremlinTsundere.blockAmt = m.block_amount;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        (void)num;
        set_monster_move(g, index, 1, Intent::Defend);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
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
            if (alive > 1) set_monster_move(g, index, 1, Intent::Defend);
            else set_monster_move(g, index, 2, Intent::Attack, m.damage[0]);
        } else {
            queue_damage(g, index, player_target, m.damage[0]);
            queue_monster_move(g, index, 2, Intent::Attack, m.damage[0]);
        }
    }
};
}
