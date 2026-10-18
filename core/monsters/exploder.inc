#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/change_state.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Exploder : MonsterHooks {
    SPIRE_STATE(Exploder, int attackDmg; int turnCount;)
    SPIRE_FN static constexpr MonsterData data() { return {30, 35, {11}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) stats.hp_max = 30;
        if (g.run.ascension < 2) stats.damage[0] = 9;
        Monster m = make_monster(g, MonsterId::Exploder, stats, hp_override);
        m.state.Exploder.attackDmg = monster_base_damage(g, m, 0);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Explosive, 3);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        const Monster& m = g.monsters[owner];
        if (m.state.Exploder.turnCount < 2) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, owner, 2, Intent::Unknown);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        ++m.state.Exploder.turnCount;
        if (m.move == 1) { add_bottom(g, actions::AnimateSlowAttack::make(owner)); queue_monster_damage(g, owner, player_target, 0); }
        queue_monster_roll(g, owner);
    }
};
}
