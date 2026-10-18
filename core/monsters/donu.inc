#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/change_state.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Donu : MonsterHooks {
    SPIRE_STATE(Donu, int beamDmg; bool isAttacking;)
    SPIRE_FN static constexpr MonsterData data() { return {265, 265, {12}, 0, 0, 1, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 9) stats.hp_min = stats.hp_max = 250;
        if (g.run.ascension < 4) stats.damage[0] = 10;
        Monster m = make_monster(g, MonsterId::Donu, stats, hp_override);
        position_monster(m, 100.0f);
        m.state.Donu.beamDmg = monster_base_damage(g, m, 0); m.state.Donu.isAttacking = false;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Artifact, g.run.ascension >= 19 ? 3 : 2);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        const Monster& m = g.monsters[owner];
        if (m.state.Donu.isAttacking) set_monster_move(g, owner, 0, Intent::Attack, monster_base_damage(g, m, 0), 2);
        else set_monster_move(g, owner, 2, Intent::Buff);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 0) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            for (int i = 0; i < 2; ++i) queue_monster_damage(g, owner, player_target, 0);
            m.state.Donu.isAttacking = false;
        } else {
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int target = g.monster_order[i];
                Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
                queue_power(g, owner, target, PowerId::Strength, 3);
            }
            m.state.Donu.isAttacking = true;
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
