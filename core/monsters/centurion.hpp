#pragma once
#include "base.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct Centurion : MonsterHooks {
    SPIRE_STATE(Centurion, int slashDmg; int furyDmg; int furyHits; int blockAmount; int BLOCK_AMOUNT; int A_17_BLOCK_AMOUNT;)
    SPIRE_FN static constexpr MonsterData data() { return {78, 83, {14, 7}, 20, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 76; stats.hp_max = 80; }
        if (g.run.ascension < 2) { stats.damage[0] = 12; stats.damage[1] = 6; }
        Monster m = make_monster(g, MonsterId::Centurion, stats, hp_override);
        auto& s = m.state.Centurion;
        s.slashDmg = m.damage[0]; s.furyDmg = m.damage[1]; s.furyHits = 3;
        s.blockAmount = g.run.ascension >= 17 ? 20 : 15; s.BLOCK_AMOUNT = 15; s.A_17_BLOCK_AMOUNT = 20;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        if ((num >= 65 && !last_two_moves(m, 2) && !last_two_moves(m, 3)) || last_two_moves(m, 1)) {
            int count = 0;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const Creature& c = g.monsters[g.monster_order[i]].creature;
                if (!c.dying && !c.escaped) ++count;
            }
            if (count > 1) set_monster_move(g, owner, 2, Intent::Defend);
            else set_monster_move(g, owner, 3, Intent::Attack, m.damage[1], m.state.Centurion.furyHits);
        } else set_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 2) { Effect e{}; e.kind = EffectKind::BlockRandomMonster; e.source = owner; e.amount = m.state.Centurion.blockAmount; add_bottom(g, e); }
        else for (int i = 0; i < (m.move == 3 ? m.state.Centurion.furyHits : 1); ++i) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            add_bottom(g, actions::ChangeState::make(g, owner, "MACE_HIT"));
            queue_damage(g, owner, player_target, m.damage[m.move == 3 ? 1 : 0]);
        }
        queue_monster_roll(g, owner);
    }
};
}
