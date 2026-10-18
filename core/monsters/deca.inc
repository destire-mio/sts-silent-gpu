#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/change_state.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Deca : MonsterHooks {
    SPIRE_STATE(Deca, int beamDmg; bool isAttacking;)
    SPIRE_FN static constexpr MonsterData data() { return {265, 265, {12}, 0, 0, 1, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 9) stats.hp_min = stats.hp_max = 250;
        if (g.run.ascension < 4) stats.damage[0] = 10;
        Monster m = make_monster(g, MonsterId::Deca, stats, hp_override);
        position_monster(m, -350.0f);
        m.state.Deca.beamDmg = monster_base_damage(g, m, 0); m.state.Deca.isAttacking = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Artifact, g.run.ascension >= 19 ? 3 : 2);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        const Monster& m = g.monsters[owner];
        if (m.state.Deca.isAttacking) set_monster_move(g, owner, 0, Intent::AttackDebuff, monster_base_damage(g, m, 0), 2);
        else set_monster_move(g, owner, 2, g.run.ascension >= 19 ? Intent::DefendBuff : Intent::Defend);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 0) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            for (int i = 0; i < 2; ++i) queue_monster_damage(g, owner, player_target, 0);
            queue_monster_card(g, CardId::Dazed, EffectKind::AddCardDiscard, 2);
            m.state.Deca.isAttacking = false;
        } else {
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int target = g.monster_order[i];
                Effect e{}; e.kind = EffectKind::Block; e.source = owner; e.target = target; e.amount = 16; add_bottom(g, e);
                if (g.run.ascension >= 19) queue_power(g, owner, target, PowerId::PlatedArmor, 3);
            }
            m.state.Deca.isAttacking = true;
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics) {
        monster_die_base(g, owner, trigger_relics);
        if (monsters_basically_dead(g)) {
            monster_onBossVictoryLogic(g, owner);
            monster_onFinalBossVictoryLogic(g, owner);
        }
    }
};
}
