#pragma once
#include "base.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct Byrd : MonsterHooks {
    SPIRE_STATE(Byrd, bool firstMove; bool isFlying;)
    SPIRE_SNAPSHOT("Byrd.peckDmg", scalar, 1)
    SPIRE_SNAPSHOT("Byrd.peckCount", scalar, game.run.ascension >= 2 ? 6 : 5)
    SPIRE_SNAPSHOT("Byrd.swoopDmg", scalar, monster.damage[1])
    SPIRE_SNAPSHOT("Byrd.flightAmt", scalar, game.run.ascension >= 17 ? 4 : 3)
    SPIRE_FN static constexpr MonsterData data() { return {26, 33, {1, 14, 3}, 0, 0, 3}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 25; stats.hp_max = 31; }
        if (g.run.ascension < 2) stats.damage[1] = 12;
        Monster m = make_monster(g, MonsterId::Byrd, stats, hp_override);
        auto& s = m.state.Byrd;
        s.firstMove = s.isFlying = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::Flight, g.run.ascension >= 17 ? 4 : 3); }
    SPIRE_FN static void changeState(Game& g, int owner, const char* key) {
        if (same_id(key, "GROUNDED")) {
            set_monster_move(g, owner, 4, Intent::Stun); monster_createIntent(g, owner);
            g.monsters[owner].state.Byrd.isFlying = false;
        }
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner]; auto& s = m.state.Byrd;
        int move;
        if (s.firstMove) { s.firstMove = false; move = g.rng.ai.randomBoolean(0.375f) ? 6 : 1; }
        else if (!s.isFlying) move = 5;
        else if (num < 50) {
            if (last_two_moves(g, m, 1)) move = g.rng.ai.randomBoolean(0.4f) ? 3 : 6;
            else move = 1;
        } else if (num < 70) {
            if (last_move(g, m, 3)) move = g.rng.ai.randomBoolean(0.375f) ? 6 : 1;
            else move = 3;
        } else if (last_move(g, m, 6)) move = g.rng.ai.randomBoolean(0.2857f) ? 3 : 1;
        else move = 6;
        if (move == 6) set_monster_move(g, owner, 6, Intent::Buff);
        else set_monster_move(g, owner, move, Intent::Attack, m.damage[move == 1 ? 0 : move == 3 ? 1 : 2], move == 1 ? (g.run.ascension >= 2 ? 6 : 5) : 1);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner]; auto& s = m.state.Byrd;
        if (m.move == 1) {
            add_bottom(g, actions::AnimateFastAttack::make(owner));
            for (int i = 0; i < (g.run.ascension >= 2 ? 6 : 5); ++i) {
                Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
                queue_damage(g, owner, player_target, m.damage[0]);
            }
        } else if (m.move == 5) {
            add_bottom(g, actions::AnimateSlowAttack::make(owner));
            queue_damage(g, owner, player_target, m.damage[2]); set_monster_move(g, owner, 2, Intent::Unknown); return;
        } else if (m.move == 2) {
            s.isFlying = true; add_bottom(g, actions::ChangeState::make(g, owner, "FLYING"));
            queue_power(g, owner, owner, PowerId::Flight, (g.run.ascension >= 17 ? 4 : 3));
        } else if (m.move == 6) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            add_bottom(g, actions::Talk::make(owner)); queue_power(g, owner, owner, PowerId::Strength, 1);
        }
        else if (m.move == 3) { add_bottom(g, actions::AnimateSlowAttack::make(owner)); queue_damage(g, owner, player_target, m.damage[1]); }
        else if (m.move == 4) { add_bottom(g, actions::SetAnimation::make(owner)); add_bottom(g, actions::TextAboveCreature::make(owner)); }
        queue_monster_roll(g, owner);
    }
};
}
