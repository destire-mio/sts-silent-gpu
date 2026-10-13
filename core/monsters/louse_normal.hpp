#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct LouseNormal : MonsterHooks {
    SPIRE_STATE(LouseNormal, bool isOpen;)
    SPIRE_FN static constexpr MonsterData data() { return {11, 16, {0, 0}, 0, 4}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::LouseNormal, data(), hp_override);
        m.state.LouseNormal.isOpen = true;
        m.damage[0] = g.rng.monster_hp.random(6, 8);
        m.damage_output[0] = m.damage[0];
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 25 && !last_move(m, 4)) || (num >= 25 && last_two_moves(m, 3)))
            set_monster_move(g, index, 4, Intent::Buff);
        else set_monster_move(g, index, 3, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            if (!m.state.LouseNormal.isOpen) {
                queue_monster_state(g, index, MonsterStateChange::Open);
                queue_wait(g, 0.5f);
            }
            queue_animation(g, index, EffectKind::AnimateSlowAttack);
            queue_damage(g, index, player_target, m.damage[0], DamageType::Normal, 0, AttackEffect::BluntLight);
        } else {
            const bool open = m.state.LouseNormal.isOpen;
            queue_monster_state(g, index, open ? MonsterStateChange::RearIdle : MonsterStateChange::Rear);
            queue_wait(g, open ? 0.9f : 1.2f);
            queue_power(g, index, index, PowerId::Strength, m.magic);
        }
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void changeState(Game& g, int index, int state) {
        g.monsters[index].state.LouseNormal.isOpen = state != static_cast<int>(MonsterStateChange::Closed);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::CurlUp, g.rng.monster_hp.random(9, 12));
    }
};
}
