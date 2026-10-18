#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/intent_flash.hpp"
#include "core/actions/change_state.hpp"

namespace spire::units::monsters {
struct Centurion : MonsterHooks {
    SPIRE_SNAPSHOT("Centurion.slashDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Centurion.furyDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_SNAPSHOT("Centurion.furyHits", scalar, 3)
    SPIRE_SNAPSHOT("Centurion.blockAmount", scalar, game.run.ascension >= 17 ? 20 : 15)
    SPIRE_SNAPSHOT("Centurion.BLOCK_AMOUNT", scalar, 15)
    SPIRE_SNAPSHOT("Centurion.A_17_BLOCK_AMOUNT", scalar, 20)
    SPIRE_FN static constexpr MonsterData data() { return {78, 83, {14, 7}, 20, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 76; stats.hp_max = 80; }
        if (g.run.ascension < 2) { stats.damage[0] = 12; stats.damage[1] = 6; }
        Monster m = make_monster(g, MonsterId::Centurion, stats, hp_override);

        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        if ((num >= 65 && !last_two_moves(g, m, 2) && !last_two_moves(g, m, 3)) || last_two_moves(g, m, 1)) {
            int count = 0;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const Creature& c = g.monsters[g.monster_order[i]].creature;
                if (!c.dying && !c.escaped) ++count;
            }
            if (count > 1) set_monster_move(g, owner, 2, Intent::Defend);
            else set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1), (3));
        } else set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 2) { Effect e{}; e.kind = EffectKind::BlockRandomMonster; e.source = owner; e.amount = (g.run.ascension >= 17 ? 20 : 15); add_bottom(g, e); }
        else for (int i = 0; i < (m.move == 3 ? (3) : 1); ++i) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            add_bottom(g, actions::ChangeState::make(g, owner, "MACE_HIT"));
            queue_monster_damage(g, owner, player_target, m.move == 3 ? 1 : 0);
        }
        queue_monster_roll(g, owner);
    }
};
}
