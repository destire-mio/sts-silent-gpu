#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Transient : MonsterHooks {
    SPIRE_STATE(Transient, int count; int startingDeathDmg;)
    SPIRE_FN static constexpr MonsterData data() { return {999, 999, {40, 50, 60, 70, 80, 90, 100}, 0, 0, 7}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 2) for (int i = 0; i < 7; ++i) stats.damage[i] -= 10;
        Monster m = make_monster(g, MonsterId::Transient, stats, hp_override == 0 ? 999 : hp_override);
        m.state.Transient.startingDeathDmg = monster_base_damage(g, m, 0);
        m.creature.gold = 1;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Fading, g.run.ascension >= 17 ? 6 : 5);
        queue_power(g, owner, owner, PowerId::Shifting, -1);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        const auto& s = g.monsters[owner].state.Transient;
        set_monster_move(g, owner, 1, Intent::Attack, s.startingDeathDmg + s.count * 10);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
        queue_monster_damage(g, owner, player_target, m.state.Transient.count);
        ++m.state.Transient.count;
        set_monster_move(g, owner, 1, Intent::Attack, m.state.Transient.startingDeathDmg + m.state.Transient.count * 10);
    }
};
}
