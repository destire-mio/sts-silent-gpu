#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Repulsor : MonsterHooks {
    SPIRE_STATE(Repulsor, int attackDmg; int dazeAmt;)
    SPIRE_FN static constexpr MonsterData data() { return {31, 38, {13}, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 29; stats.hp_max = 35; }
        if (g.run.ascension < 2) stats.damage[0] = 11;
        Monster m = make_monster(g, MonsterId::Repulsor, stats, hp_override);
        m.state.Repulsor.attackDmg = monster_base_damage(g, m, 0);
        m.state.Repulsor.dazeAmt = 2;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        if (num < 20 && !last_move(g, m, 2)) set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, owner, 1, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 2) { add_bottom(g, actions::AnimateSlowAttack::make(owner)); queue_monster_damage(g, owner, player_target, 0); }
        else queue_monster_card(g, CardId::Dazed, EffectKind::MakeTempCardInDrawPile, m.state.Repulsor.dazeAmt);
        queue_monster_roll(g, owner);
    }
};
}
