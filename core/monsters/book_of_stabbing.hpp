#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct BookOfStabbing : MonsterHooks {
    SPIRE_SNAPSHOT("BookOfStabbing.stabDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("BookOfStabbing.bigStabDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_STATE(BookOfStabbing, int stabCount;)
    SPIRE_FN static constexpr MonsterData data() { return {168, 172, {7, 24}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 8) { stats.hp_min = 160; stats.hp_max = 164; }
        if (g.run.ascension < 3) { stats.damage[0] = 6; stats.damage[1] = 21; }
        Monster m = make_monster(g, MonsterId::BookOfStabbing, stats, hp_override);
        m.state.BookOfStabbing.stabCount = 1;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::PainfulStabs, -1); }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner]; auto& s = m.state.BookOfStabbing;
        if ((num < 15 && !last_move(g, m, 2)) || (num >= 15 && last_two_moves(g, m, 1))) {
            set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 1));
            if (g.run.ascension >= 18) ++s.stabCount;
        } else { ++s.stabCount; set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0), s.stabCount); }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        add_bottom(g, actions::ChangeState::make(g, owner, m.move == 1 ? "ATTACK" : "ATTACK_2"));
        if (m.move == 2) queue_monster_damage(g, owner, player_target, 1);
        else for (int i = 0; i < m.state.BookOfStabbing.stabCount; ++i) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            queue_monster_damage(g, owner, player_target, 0);
        }
        queue_monster_roll(g, owner);
    }
};
}
