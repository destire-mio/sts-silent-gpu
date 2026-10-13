#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Spiker : MonsterHooks {
    SPIRE_STATE(Spiker, int attackDmg; int startingThorns; int thornsCount;)
    SPIRE_FN static constexpr MonsterData data() { return {44, 60, {9}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 42; stats.hp_max = 56; }
        if (g.run.ascension < 2) stats.damage[0] = 7;
        Monster m = make_monster(g, MonsterId::Spiker, stats, hp_override);
        m.state.Spiker.attackDmg = m.damage[0];
        m.state.Spiker.startingThorns = g.run.ascension >= 2 ? 4 : 3;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Thorns, g.monsters[owner].state.Spiker.startingThorns + (g.run.ascension >= 17 ? 3 : 0));
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        if (m.state.Spiker.thornsCount > 5 || (num < 50 && !last_move(m, 1)))
            set_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, owner, 2, Intent::Buff);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) { add_bottom(g, actions::AnimateSlowAttack::make(owner)); queue_damage(g, owner, player_target, m.damage[0]); }
        else { ++m.state.Spiker.thornsCount; queue_power(g, owner, owner, PowerId::Thorns, 2); }
        queue_monster_roll(g, owner);
    }
};
}
