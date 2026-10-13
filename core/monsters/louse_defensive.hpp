#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct LouseDefensive : MonsterHooks {
    SPIRE_STATE(LouseDefensive, int16_t biteDamage; bool isOpen;)
    SPIRE_FN static constexpr MonsterData data() { return {12, 18, {0, 0}, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 11; stats.hp_max = 17; }
        Monster m = make_monster(g, MonsterId::LouseDefensive, stats, hp_override);
        monster_base_damage(g, m, 0) = g.run.ascension >= 2 ? g.rng.monster_hp.random(6, 8) : g.rng.monster_hp.random(5, 7);
        monster_output_damage(g, m, 0) = monster_base_damage(g, m, 0);
        m.state.LouseDefensive.biteDamage = monster_base_damage(g, m, 0);
        m.state.LouseDefensive.isOpen = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 25 && !(g.run.ascension >= 17 ? last_move(g, m, 4) : last_two_moves(g, m, 4))) || (num >= 25 && last_two_moves(g, m, 3)))
            set_monster_move(g, index, 4, Intent::Debuff);
        else set_monster_move(g, index, 3, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            if (!m.state.LouseDefensive.isOpen) add_bottom(g, units::actions::ChangeState::make(g, index, "OPEN"));
            queue_damage(g, index, player_target, monster_base_damage(g, m, 0));
        } else {
            add_bottom(g, units::actions::ChangeState::make(g, index, m.state.LouseDefensive.isOpen ? "REAR_IDLE" : "REAR"));
            queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        }
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void changeState(Game& g, int index, const char* state) {
        g.monsters[index].state.LouseDefensive.isOpen = !same_id(state, "CLOSED");
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::CurlUp, g.run.ascension >= 17 ? g.rng.monster_hp.random(9, 12) : g.run.ascension >= 7 ? g.rng.monster_hp.random(4, 8) : g.rng.monster_hp.random(3, 7));
    }
};
}
