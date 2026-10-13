#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct OrbWalker : MonsterHooks {
    SPIRE_STATE(OrbWalker, int clawDmg; int laserDmg;)
    SPIRE_FN static constexpr MonsterData data() { return {92, 102, {11, 16}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        g.rng.monster_hp.random(90, 96);
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 90; stats.hp_max = 96; }
        if (g.run.ascension < 2) { stats.damage[0] = 10; stats.damage[1] = 15; }
        Monster m = make_monster(g, MonsterId::OrbWalker, stats, hp_override);
        m.state.OrbWalker.clawDmg = m.damage[1];
        m.state.OrbWalker.laserDmg = m.damage[0];
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::GenericStrengthUp, g.run.ascension >= 17 ? 5 : 3);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        if ((num < 40 && !last_two_moves(m, 2)) || (num >= 40 && last_two_moves(m, 1)))
            set_monster_move(g, owner, 2, Intent::Attack, m.damage[1]);
        else set_monster_move(g, owner, 1, Intent::AttackDebuff, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 2) add_bottom(g, actions::AnimateSlowAttack::make(owner));
        else add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
        queue_damage(g, owner, player_target, m.damage[m.move == 2 ? 1 : 0]);
        if (m.move == 1) queue_monster_card(g, CardId::Burn, EffectKind::MakeTempCardInDiscardAndDeck, 1);
        queue_monster_roll(g, owner);
    }
};
}
