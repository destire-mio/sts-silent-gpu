#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct TimeEater : MonsterHooks {
    SPIRE_SNAPSHOT("TimeEater.reverbDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("TimeEater.headSlamDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_STATE(TimeEater, bool usedHaste; bool firstTurn;)
    SPIRE_FN static constexpr MonsterData data() { return {480, 480, {8, 32}, 0, 0, 2, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 9) stats.hp_min = stats.hp_max = 456;
        if (g.run.ascension < 4) { stats.damage[0] = 7; stats.damage[1] = 26; }
        Monster m = make_monster(g, MonsterId::TimeEater, stats, hp_override);
        position_monster(m, -50.0f);
        m.state.TimeEater.firstTurn = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::TimeWarp, 0); }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (m.creature.hp < m.creature.max_hp / 2 && !m.state.TimeEater.usedHaste) {
            m.state.TimeEater.usedHaste = true; set_monster_move(g, owner, 5, Intent::Buff); return;
        }
        for (;;) {
            if (num < 45) {
                if (!last_two_moves(g, m, 2)) { set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0), 3); return; }
                num = g.rng.ai.random(50, 99);
            } else if (num < 80) {
                if (!last_move(g, m, 4)) set_monster_move(g, owner, 4, Intent::AttackDebuff, monster_base_damage(g, m, 1));
                else if (g.rng.ai.randomBoolean(0.66f)) set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0), 3);
                else set_monster_move(g, owner, 3, Intent::DefendDebuff);
                return;
            } else if (!last_move(g, m, 3)) { set_monster_move(g, owner, 3, Intent::DefendDebuff); return; }
            else num = g.rng.ai.random(74);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.state.TimeEater.firstTurn) { add_bottom(g, actions::Talk::make(owner)); m.state.TimeEater.firstTurn = false; }
        if (m.move == 2) for (int i = 0; i < 3; ++i) {
            add_bottom(g, actions::VFX::make(owner));
            queue_monster_damage(g, owner, player_target, 0);
        }
        else if (m.move == 3) {
            queue_block(g, owner, 20);
            queue_power(g, owner, player_target, PowerId::Vulnerable, 1, true);
            queue_power(g, owner, player_target, PowerId::Weak, 1, true);
            if (g.run.ascension >= 19) queue_power(g, owner, player_target, PowerId::Frail, 1, true);
        } else if (m.move == 4) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            queue_monster_damage(g, owner, player_target, 1);
            queue_power(g, owner, player_target, PowerId::DrawReduction, 1);
            if (g.run.ascension >= 19) queue_monster_card(g, CardId::Slimed, EffectKind::AddCardDiscard, 2);
        } else {
            add_bottom(g, actions::Shout::make(owner));
            Effect e{}; e.kind = EffectKind::RemoveDebuffsAction; e.target = owner; add_bottom(g, e);
            e = {}; e.kind = EffectKind::RemovePower; e.source = e.target = owner; e.power = PowerId::GainStrength; add_bottom(g, e);
            e = {}; e.kind = EffectKind::Heal; e.source = e.target = owner; e.amount = m.creature.max_hp / 2 - m.creature.hp; add_bottom(g, e);
            if (g.run.ascension >= 19) queue_block(g, owner, monster_base_damage(g, m, 1));
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics) {
        if (!g.cannot_lose) {
            monster_die_base(g, owner, trigger_relics);
            monster_onBossVictoryLogic(g, owner); monster_onFinalBossVictoryLogic(g, owner);
        }
    }
};
}
