#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct WrithingMass : MonsterHooks {
    SPIRE_STATE(WrithingMass, int normalDebuffAmt; bool firstMove; bool usedMegaDebuff;)
    SPIRE_FN static constexpr MonsterData data() { return {175, 175, {38, 9, 16, 12}, 0, 0, 4}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) stats.hp_min = stats.hp_max = 160;
        if (g.run.ascension < 2) { stats.damage[0] = 32; stats.damage[1] = 7; stats.damage[2] = 15; stats.damage[3] = 10; }
        Monster m = make_monster(g, MonsterId::WrithingMass, stats, hp_override);
        m.state.WrithingMass.normalDebuffAmt = 2; m.state.WrithingMass.firstMove = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Reactive, -1);
        queue_power(g, owner, owner, PowerId::Malleable, 3);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner]; auto& s = m.state.WrithingMass;
        if (s.firstMove) {
            s.firstMove = false;
            if (num < 33) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 1), 3);
            else if (num < 66) set_monster_move(g, owner, 2, Intent::AttackDefend, monster_base_damage(g, m, 2));
            else set_monster_move(g, owner, 3, Intent::AttackDebuff, monster_base_damage(g, m, 3));
            return;
        }
        int move = -1;
        while (move < 0) {
            if (num < 10) {
                if (!last_move(g, m, 0)) move = 0;
                else num = g.rng.ai.random(10, 99);
            } else if (num < 20) {
                if (!s.usedMegaDebuff && !last_move(g, m, 4)) move = 4;
                else if (g.rng.ai.randomBoolean(0.1f)) move = 0;
                else num = g.rng.ai.random(20, 99);
            } else if (num < 40) {
                if (!last_move(g, m, 3)) move = 3;
                else if (g.rng.ai.randomBoolean(0.4f)) num = g.rng.ai.random(19);
                else num = g.rng.ai.random(40, 99);
            } else if (num < 70) {
                if (!last_move(g, m, 1)) move = 1;
                else if (g.rng.ai.randomBoolean(0.3f)) move = 2;
                else num = g.rng.ai.random(39);
            } else if (!last_move(g, m, 2)) move = 2;
            else num = g.rng.ai.random(69);
        }
        if (move == 4) set_monster_move(g, owner, 4, Intent::StrongDebuff);
        else set_monster_move(g, owner, move, move == 2 ? Intent::AttackDefend : move == 3 ? Intent::AttackDebuff : Intent::Attack, monster_base_damage(g, m, move), move == 1 ? 3 : 1);
        monster_createIntent(g, owner);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 4) {
            m.state.WrithingMass.usedMegaDebuff = true;
            add_bottom(g, actions::FastShake::make(owner));
            queue_monster_card(g, CardId::Parasite, EffectKind::AddCardToDeck, 0);
        } else {
            if (m.move == 0) add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            else if (m.move == 1) add_bottom(g, actions::AnimateSlowAttack::make(owner));
            else if (m.move == 2) add_bottom(g, actions::AnimateFastAttack::make(owner));
            for (int i = 0; i < (m.move == 1 ? 3 : 1); ++i) queue_monster_damage(g, owner, player_target, m.move);
            if (m.move == 2) queue_block(g, owner, monster_base_damage(g, m, 2));
            else if (m.move == 3) {
                queue_power(g, owner, player_target, PowerId::Weak, m.state.WrithingMass.normalDebuffAmt, true);
                queue_power(g, owner, player_target, PowerId::Vulnerable, m.state.WrithingMass.normalDebuffAmt, true);
                add_bottom(g, actions::AnimateFastAttack::make(owner));
            }
        }
        queue_monster_roll(g, owner);
    }
};
}
